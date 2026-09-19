using System;
using System.IO.Ports;
using System.Runtime.InteropServices;
using System.Threading;

namespace ReaderManager
{
    /// <summary>
    /// MCU OTA 下载器。两条线路的偏移语义必须与 python 参考实现逐条对应：
    ///   · 串口 UART1：参考 tools/ota_send.py —— 偏移为【含 82B 包头的绝对偏移】
    ///     （total=len(pkg)，包头帧 seq=0 只发一次，设备不校验 seq，ACK 回绝对进度）
    ///   · USB-CDC ：参考 tools/ota_usb_stream_send.py —— 偏移为【载荷相对偏移】
    ///     （total=len(pkg)-82，设备严格校验 seq=s_off/4096+1，ACK/probe 回 s_off）
    /// 共用帧协议："OTA1"+type(0x50/0x51/0x52)+seq(2LE)+len(2LE)+payload+CRC16-CCITT-FALSE
    /// 卡死保护：任何阶段连续 10s 无设备进度推进即中止（UI 按钮随之恢复，无需强杀应用）。
    /// </summary>
    public class OtaUpdater
    {
        public event Action<int, int> ProgressChanged;   // (offset, total) 与通道基址一致
        public event Action<string> StatusChanged;

        private SerialPort _sp;
        private WinUsbChannel _winusb;         /* v1.10: WinUSB 通道（绕过 usbser.sys 265ms） */
        private bool _isUsbCdc;
        private bool _isWinUsb;                /* v1.10 */
        private int _batch = 1;
        private volatile bool _stop = false;   /* 用户停止请求 */

        /* v1.10: WinUSB 批量读缓冲（WinUsb_ReadPipe 一次 64B，逐字节状态机消费） */
        private byte[] _winusbRx = new byte[64];
        private int _winusbRxPos = 0;
        private int _winusbRxLen = 0;

        public bool IsOpen { get { return _isWinUsb ? (_winusb != null && _winusb.IsOpen) : (_sp != null && _sp.IsOpen); } }

        private void Status(string s) { StatusChanged?.Invoke(s); }

        /* v1.10: 统一写（CDC/WinUSB） */
        private bool WriteAll(byte[] buf, int off, int len)
        {
            if (_isWinUsb)
                return _winusb != null && _winusb.Write(buf, off, len);
            _sp.Write(buf, off, len);
            return true;
        }

        /* v1.10: 统一读 1 字节（CDC: ReadByte；WinUSB: 批量缓冲消费）；无数据返回 -1 */
        private int ReadByte1()
        {
            if (!_isWinUsb)
            {
                /* v1.30 已回退：阻塞读实测无效（干净测试仍 531ms），且对串口有 1000ms 阻塞副作用 */
                if (_sp.BytesToRead > 0)
                    return _sp.ReadByte();
                return -1;
            }
            if (_winusbRxPos >= _winusbRxLen)
            {
                int n = _winusb.Read(_winusbRx, 0, _winusbRx.Length);
                if (n <= 0) return -1;
                _winusbRxPos = 0;
                _winusbRxLen = n;
            }
            return _winusbRx[_winusbRxPos++];
        }

        /* v1.21: 方案 A——usbser.sys 内部缓冲等待上报的 200-500ms 延迟，
         * COMMTIMEOUTS: ReadIntervalTimeout=MAXDWORD + 10ms 常量超时 → 有数据立即返回 */
        [StructLayout(LayoutKind.Sequential)]
        struct COMMTIMEOUTS
        {
            public uint ReadIntervalTimeout;
            public uint ReadTotalTimeoutMultiplier;
            public uint ReadTotalTimeoutConstant;
            public uint WriteTotalTimeoutMultiplier;
            public uint WriteTotalTimeoutConstant;
        }

        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool SetCommTimeouts(IntPtr hFile, ref COMMTIMEOUTS lpCommTimeouts);

        private static void ApplyCommTimeouts(SerialPort sp)
        {
            try
            {
                IntPtr h = IntPtr.Zero;
                var bs = sp.BaseStream;
                var t = bs.GetType();
                var f = t.GetField("_handle", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance)
                     ?? t.GetField("m_fileHandle", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance);
                if (f != null)
                {
                    var sfh = f.GetValue(bs) as Microsoft.Win32.SafeHandles.SafeFileHandle;
                    if (sfh != null) h = sfh.DangerousGetHandle();
                }
                else
                {
                    var p = t.GetProperty("SafeFileHandle", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Public | System.Reflection.BindingFlags.Instance);
                    if (p != null)
                    {
                        var sfh = p.GetValue(bs) as Microsoft.Win32.SafeHandles.SafeFileHandle;
                        if (sfh != null) h = sfh.DangerousGetHandle();
                    }
                }
                if (h == IntPtr.Zero || h == new IntPtr(-1)) return;
                COMMTIMEOUTS ct = new COMMTIMEOUTS();
                /* v1.21: 官方"立即返回"组合——MAXDWORD + Constant=0 + Multiplier=0：
                 * ReadFile 立即返回已收字符（MAXDWORD+非零 Constant 是"等待模式"，反而引入延迟） */
                ct.ReadIntervalTimeout = 0xFFFFFFFF;   /* MAXDWORD */
                ct.ReadTotalTimeoutMultiplier = 0;
                ct.ReadTotalTimeoutConstant = 0;
                ct.WriteTotalTimeoutMultiplier = 0;
                ct.WriteTotalTimeoutConstant = 3000;
                SetCommTimeouts(h, ref ct);
            }
            catch { /* 失败不影响主流程 */ }
        }

        public bool Open(string port, int baud, bool isUsbCdc)
        {
            _isWinUsb = false;
            try
            {
                _sp = new SerialPort(port, baud, Parity.None, 8, StopBits.One);
                _sp.ReadTimeout = 1000;
                _sp.WriteTimeout = 3000;
                _sp.Open();
                /* v1.24: COMMTIMEOUTS 临时禁用——验证它是否导致 CDC R 110ms->531ms（8.2s->33s 回归元凶） */
                // ApplyCommTimeouts(_sp);
                _sp.DiscardInBuffer();
                _isUsbCdc = isUsbCdc;
                _batch = isUsbCdc ? 2 : 1;   /* USB 批 2 帧（8KB RX 缓冲 + 设备每 2 帧 ACK）；串口逐帧 */
                _stop = false;               /* 新会话复位停止标志 */
                return true;
            }
            catch (Exception ex)
            {
                Status("open " + port + " fail: " + ex.Message);
                return false;
            }
        }

        /* v1.10: WinUSB 通道打开（设备 hw_inf=7 WinUSB 模式，VID 0x2E88/PID 0x4608；4607 曾被 Windows 缓存失败枚举，故用 4608） */
        public bool OpenWinUsb()
        {
            Close();
            _isWinUsb = true;
            _isUsbCdc = true;              /* 载荷相对偏移语义与 USB-CDC 相同 */
            _batch = 2;                    /* 与设备 ACK 每 2 帧匹配 */
            _winusb = new WinUsbChannel();
            _winusb.StatusChanged += s => Status(s);
            _winusbRxPos = 0; _winusbRxLen = 0;
            _stop = false;
            return _winusb.Open(0x2E88, 0x4608);   /* v1.10: PID 4608（4607 被 Windows 缓存失败枚举） */
        }

        public void Close()
        {
            if (_winusb != null) { try { _winusb.Close(); } catch { } _winusb = null; }
            try { if (_sp != null && _sp.IsOpen) _sp.Close(); } catch { }
        }

        /// <summary>
        /// v1.15: WinUSB 回环吞吐测试——连续写 chunkSize 字节并读回（chunkSize 须为 64 倍数）。
        /// 验证 winusb.sys 对连续流是否也有 ~500ms/笔取走周期（对照 OTA 稀疏 ACK 的 531ms）。
        /// </summary>
        public string WinUsbEchoTest(int chunks, int chunkSize)
        {
            if (_winusb == null || !_winusb.IsOpen) return "WinUSB not open";
            try
            {
                byte[] tx = new byte[chunkSize];
                for (int i = 0; i < tx.Length; i++) tx[i] = (byte)(i & 0xFF);
                byte[] rx = new byte[chunkSize];
                long t0 = Environment.TickCount;
                long worst = 0;
                for (int c = 0; c < chunks; c++)
                {
                    long t1 = Environment.TickCount;
                    if (!_winusb.Write(tx, 0, tx.Length))
                    { return "echo write fail @" + c; }   /* 不 Close——通道由调用方统一管理 */
                    int rxGot = 0;
                    int deadline = (int)Environment.TickCount + 3000;
                    while (rxGot < chunkSize && Environment.TickCount < deadline)
                    {
                        int n = _winusb.Read(rx, rxGot, chunkSize - rxGot);
                        if (n <= 0) { Thread.Sleep(1); continue; }
                        rxGot += n;
                    }
                    if (rxGot < chunkSize)
                    { return "echo read short " + rxGot + "/" + chunkSize + " @" + c; }   /* 不 Close——通道由调用方统一管理 */
                    long t3 = Environment.TickCount;
                    if (t3 - t1 > worst) worst = t3 - t1;
                }
                long tEnd = Environment.TickCount;
                long totalBytes = (long)chunks * chunkSize;
                double secs = (tEnd - t0) / 1000.0;
                string rep = string.Format("echo {0}x{1}B: {2}ms total, {3:F0} KB/s, avg {4:F1}ms/iter, worst {5}ms",
                    chunks, chunkSize, tEnd - t0,
                    (totalBytes / 1024.0) / (secs > 0 ? secs : 0.001),
                    (double)(tEnd - t0) / chunks, worst);
                return rep;   /* 通道由调用方统一 Close */
            }
            catch (Exception ex)
            {
                return "echo ex: " + ex.Message;
            }
        }

        /// <summary>请求立即停止：置标志 + 关闭串口（令 ReadAck/Write 立即返回）</summary>
        public void Stop()
        {
            _stop = true;
            Close();
        }

        /// <summary>
        /// 查询设备固件版本（OTA1 协议扩展 "VER1" 帧）：设备回 ACK(4B 版本)。
        /// 成功返回版本号（如 0x01150400）；失败返回 -1。
        /// </summary>
        public int QueryVersion(string port, int baud, bool isUsbCdc)
        {
            if (!Open(port, baud, isUsbCdc)) return -1;
            try
            {
                /* 用 RESUME 类型：旧固件忽略非 DATA 帧 → 查询安全（不会误写暂存/触发重启） */
                byte[] q = OtaProtocol.MakeFrame(OtaProtocol.TypeResume, 0xFFFF,
                                                 System.Text.Encoding.ASCII.GetBytes("VER1"));
                WriteAll(q, 0, q.Length);
                int ver = ReadAck(3000);
                Close();
                return ver;
            }
            catch { Close(); return -1; }
        }

        public bool Update(byte[] pkg)
        {
            if (pkg == null || pkg.Length < OtaProtocol.PkgHdrLen)
            {
                Status("bad package (need >= 82B OTA header)");
                return false;
            }
            if (!IsOpen) { Status("channel not open"); return false; }   /* v1.10: 统一检查（CDC/WinUSB） */

            byte[] hdr = new byte[OtaProtocol.PkgHdrLen];
            Buffer.BlockCopy(pkg, 0, hdr, 0, hdr.Length);
            /* 通道基址：串口=含包头绝对偏移（0 起）；USB=载荷相对偏移（82 起） */
            int baseOff = _isUsbCdc ? OtaProtocol.PkgHdrLen : 0;
            int total = _isUsbCdc ? pkg.Length - OtaProtocol.PkgHdrLen : pkg.Length;

            /* 1) 包头帧握手（两线路行为不同，见 SendHeader） */
            int devOff;
            if (!SendHeader(hdr, out devOff))
            {
                Status("no ACK after header (device prepare failed?)");
                return false;
            }
            if (devOff >= total) { Status("device already at " + devOff + " >= " + total); return true; }
            if (devOff > 0) Status("device ACK, resume from " + devOff + " / " + total + " ...");
            else if (_isUsbCdc) Status("device ACK(0), streaming " + total + " bytes...");
            else Status("header no ACK (device busy/not listening?), fallback stream from 0");

            /* 卡死保护：设备已确认的最大偏移 10s 不推进即中止 */
            int lastMove = Environment.TickCount;
            int ackedMax = 0;

            /* 2) 数据流：seq = 相对偏移/4096 + 1（USB 设备严格校验；串口不校验，同一公式无妨） */
            while (devOff < total && !_stop)
            {
                if (Environment.TickCount - lastMove > 10000)
                {
                    Status("stalled >10s (device acked " + ackedMax + "/" + total + "), aborting");
                    return false;
                }
                int tBatch0 = Environment.TickCount;
                for (int i = 0; i < _batch && devOff < total; i++)
                {
                    int chunkLen = Math.Min(OtaProtocol.MaxPayload, total - devOff);
                    byte[] chunk = new byte[chunkLen];
                    Buffer.BlockCopy(pkg, baseOff + devOff, chunk, 0, chunkLen);
                    ushort seq = (ushort)(devOff / OtaProtocol.MaxPayload + 1);
                    byte[] frame = OtaProtocol.MakeFrame(OtaProtocol.TypeData, seq, chunk);
                    WriteAll(frame, 0, frame.Length);   /* 必须写 frame.Length——count=0 是空操作！ */
                    devOff += chunkLen;
                }
                int tAfterWrite = Environment.TickCount;
                int ack = ReadAck(3000);
                if (ack < 0)
                {
                    /* 超时 → probe 查设备真实进度，跟随其偏移（与 python 一致） */
                    if (Environment.TickCount - lastMove > 10000)
                    {
                        Status("stalled >10s, aborting");
                        return false;
                    }
                    Status("timeout, probing device progress...");
                    WriteProbe();
                    int p = ReadAck(2500);
                    if (p >= 0)
                    {
                        if (p > ackedMax) { ackedMax = p; lastMove = Environment.TickCount; }
                        if (p >= total) { devOff = total; ProgressChanged?.Invoke(devOff, total); break; }
                        devOff = p;
                        ProgressChanged?.Invoke(devOff, total);
                        Status("probe: device at " + p + ", resend from there");
                    }
                    else Status("probe no response, retry");
                }
                else
                {
                    /* python: 始终跟随设备偏移（可前进也可回退） */
                    if (ack > ackedMax) { ackedMax = ack; lastMove = Environment.TickCount; }
                    if (ack >= total) { devOff = total; ProgressChanged?.Invoke(devOff, total); break; }
                    devOff = ack;
                    ProgressChanged?.Invoke(devOff, total);
                }
                /* v9.82d: 每 10 批打印批耗时（W=Write R=ReadAck），简洁模式 */
                if ((devOff / 4096) % 10 == 0)
                    Status(string.Format("batch off={0} W={1}ms R={2}ms",
                        devOff, tAfterWrite - tBatch0, Environment.TickCount - tAfterWrite));
            }

            /* 3) 完成阶段 */
            if (_isUsbCdc)
            {
                /* python ota_usb_stream_send.py: probe + 补发缺口，直到设备报 offset>=total 或重启 */
                Status("device complete, verifying+swap+reset...");
                int noresp = 0;
                int lastP = -1;
                int pSince = Environment.TickCount;
                for (int round = 0; round < 300 && !_stop; round++)
                {
                    if (Environment.TickCount - pSince > 10000)
                    {
                        Status("device stalled >10s at " + lastP + "/" + total + ", aborting");
                        return false;
                    }
                    try { if (!_isWinUsb) _sp.DiscardInBuffer(); _winusbRxPos = _winusbRxLen = 0; } catch { return true; }
                    /* v9.82d: 探测写失败 = 设备已断开（复位后重枚举）→ 立即判定成功，不再等 10s */
                    if (!WriteProbe())
                    {
                        Status("device gone (reset after swap)");
                        return true;
                    }
                    int p = ReadAck(1000);
                    if (p < 0)
                    {
                        noresp++;
                        /* ~4s 无响应 → 设备已重启（swap 后复位） */
                        if (noresp >= 4) { Status("device gone (reset after swap)"); return true; }
                    }
                    else
                    {
                        noresp = 0;
                        if (p >= total)
                        {
                            Status("device offset " + p + " >= " + total + ", verifying+swap+reset...");
                            return true;
                        }
                        if (p != lastP) { lastP = p; pSince = Environment.TickCount; }
                        ProgressChanged?.Invoke(p, total);
                        /* 补发缺口（载荷偏移 p → 包内绝对 baseOff+p） */
                        int chunkLen = Math.Min(OtaProtocol.MaxPayload, total - p);
                        byte[] chunk = new byte[chunkLen];
                        Buffer.BlockCopy(pkg, baseOff + p, chunk, 0, chunkLen);
                        ushort seq = (ushort)(p / OtaProtocol.MaxPayload + 1);
                        byte[] rf = OtaProtocol.MakeFrame(OtaProtocol.TypeData, seq, chunk);
                        try { WriteAll(rf, 0, rf.Length); } catch { return true; }
                    }
                    Thread.Sleep(100);
                }
                return true;
            }
            else
            {
                /* python ota_send.py: 进度满后发一次探测帧兜底即返回（设备自行 finish+复位） */
                WriteProbe();
                Status("all bytes sent, device verifying+swap+reset...");
                return true;
            }
        }

        /// <summary>
        /// 包头帧握手：
        ///   · USB（python ota_usb_stream_send.py）：burst 5 帧 × 2 轮，每轮 10s 超时；
        ///     设备对流式会话中的重复包头只回 ACK(s_off) 不重置（v9.80 保护），安全。
        ///   · 串口（python ota_send.py）：包头帧【只发一次】，4s 无响应则从 0 直接开始
        ///     （首数据帧含包头字节，设备按会话首帧处理）。
        ///     注意：串口设备【没有】重复包头保护，burst 会把包头重复写入载荷（曾致卡 41370）。
        /// </summary>
        private bool SendHeader(byte[] hdr, out int devOff)
        {
            devOff = 0;
            byte[] frame = OtaProtocol.MakeFrame(OtaProtocol.TypeData, 0, hdr);
            if (_isUsbCdc)
            {
                for (int attempt = 0; attempt < 2; attempt++)
                {
                    try
                    {
                        for (int i = 0; i < 5; i++) WriteAll(frame, 0, frame.Length);
                        int ack = ReadAck(10000);
                        if (ack >= 0) { devOff = ack; return true; }   /* ACK(0) 就绪；ACK(s_off) 中途中继 */
                    }
                    catch { }
                }
                return false;
            }
            else
            {
                try
                {
                    WriteAll(frame, 0, frame.Length);
                    int ack = ReadAck(4000);
                    if (ack >= 0)
                    {
                        devOff = ack;
                        if (devOff < OtaProtocol.PkgHdrLen) devOff = OtaProtocol.PkgHdrLen;  /* 设备回 ACK(82) */
                        return true;
                    }
                }
                catch { }
                devOff = 0;   /* 无响应：从 0 起发（python 同款兜底） */
                return true;
            }
        }

        /// <summary>len=0 DATA 探测帧：设备回 ACK(当前进度)。返回写是否成功（false=设备已断开）。</summary>
        private bool WriteProbe()
        {
            byte[] pf = OtaProtocol.MakeFrame(OtaProtocol.TypeData, 0xFFFF, null);
            try { return WriteAll(pf, 0, pf.Length); } catch { return false; }
        }

        /* 设备 TRACE 行缓冲（printf 与 OTA 帧共用一条串口线，python read_frame 同款处理） */
        private readonly System.Text.StringBuilder _traceLine = new System.Text.StringBuilder();

        private void Trace(byte b)
        {
            if (b == 0) return;   /* v1.17: 忽略 WinUSB 保活填充字节（0x00） */
            if (b == (byte)'\n')
            {
                string line = _traceLine.ToString().Trim();
                _traceLine.Clear();
                if (line.Length > 0) Status("[dev] " + line);
            }
            else _traceLine.Append((char)b);
        }

        /// <summary>
        /// 读取一个 ACK 帧（python read_frame 风格：逐字节同步 "OTA1" magic，
        /// 非帧字节作为设备 TRACE 显示）：返回设备确认偏移；超时返回 -1
        /// </summary>
        private int ReadAck(int timeoutMs)
        {
            int deadline = Environment.TickCount + timeoutMs;
            byte[] hdr = new byte[9];   /* "OTA1"+type+seq+len */
            int hlen = 0;
            while (Environment.TickCount < deadline && !_stop)
            {
                if (!IsOpen) return -1;
                int b = ReadByte1();
                if (b < 0)
                {
                    /* v1.11: WinUSB 无 Sleep 持续 ReadPipe（RAW_IO 后每次调用即直接 URB，短包即回）；
                     * 530ms 根因已定位为非 RAW_IO 模式的批量 IN stutter（libusb #490），由 WinUsbChannel RAW_IO 修复 */
                    if (_isWinUsb) continue;
                    Thread.Sleep(5);
                    continue;
                }

                /* 帧头同步：找 "OTA1"（非帧字节作为设备 TRACE 输出） */
                if (hlen < 4)
                {
                    char expect = "OTA1"[hlen];
                    if (b == (byte)expect) { hdr[hlen++] = (byte)b; continue; }
                    for (int i = 0; i < hlen; i++) Trace(hdr[i]);  /* 已缓冲的部分匹配也属于 TRACE */
                    hlen = 0;
                    Trace((byte)b);
                    continue;
                }
                /* 帧头剩余：type + seq + len */
                hdr[hlen++] = (byte)b;
                if (hlen < 9) continue;
                hlen = 0;
                int plen = hdr[7] | (hdr[8] << 8);
                int total = OtaProtocol.FrameHdrLen + plen + OtaProtocol.FrameCrcLen;
                if (total > 64) { Status("[dev] bad frame len=" + plen); continue; }   /* ACK/RESUME 载荷仅 4B */

                /* 读 payload + CRC */
                byte[] body = new byte[total];
                Buffer.BlockCopy(hdr, 0, body, 0, 9);
                int got = 0;
                int bodyDeadline = Environment.TickCount + 2000;
                while (got < plen + OtaProtocol.FrameCrcLen && Environment.TickCount < bodyDeadline && !_stop)
                {
                    int c = ReadByte1();
                    if (c < 0) break;
                    body[9 + got++] = (byte)c;
                }
                if (got < plen + OtaProtocol.FrameCrcLen) continue;   /* 帧不完整，重同步 */

                /* CRC 校验 */
                if (OtaProtocol.Crc16(body, body.Length - 2) !=
                    (ushort)(body[body.Length - 2] | (body[body.Length - 1] << 8)))
                {
                    Status("[dev] ack crc FAIL");
                    continue;
                }
                if (body[4] != OtaProtocol.TypeAck && body[4] != OtaProtocol.TypeResume)
                {
                    Status("[dev] unexpected frame type 0x" + body[4].ToString("X2"));
                    continue;
                }
                if (plen < 4) continue;
                return body[9] | (body[10] << 8) | (body[11] << 16) | (body[12] << 24);
            }
            if (_traceLine.Length > 0)
            {
                string line = _traceLine.ToString().Trim();
                _traceLine.Clear();
                if (line.Length > 0) Status("[dev] " + line);
            }
            return -1;
        }
    }
}
