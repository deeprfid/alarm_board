using System;
using System.Collections.Generic;
using System.IO;
using ReaderManager.Models;
using System.Linq;
using System.Net.Http;
using System.Threading;
using System.Windows;
using System.Windows.Controls;
using Microsoft.Win32;
using System.IO.Ports;
using System.Diagnostics;

namespace ReaderManager
{
    /// <summary>MCU 固件升级页面：串口 / USB-CDC / HTTP / USB 盘 4 通道</summary>
    public partial class OtaUpdateView : UserControl
    {
        /* v9.82e: 日志时间戳 = 墙钟 HH:mm:ss + [ x.xxx ms]（相对本次 OTA 起点，每次开始清零） */
        private static readonly Stopwatch _sw = Stopwatch.StartNew();

        private OtaUpdater _updater;
        private bool _running;
        private CancellationTokenSource _cts;

        public OtaUpdateView()
        {
            InitializeComponent();
            Loaded += (s, e) => RefreshPorts();
            cmbChannel.SelectedIndex = 0;
            UpdateChannelUi();
        }

        private void RefreshPorts()
        {
            cmbPort.Items.Clear();
            /* 去重 + 数字排序（Windows 幽灵端口可能导致同一 COM 号出现两次） */
            List<string> ports = new List<string>();
            foreach (string p in SerialPort.GetPortNames())
                if (!ports.Contains(p)) ports.Add(p);
            ports.Sort((a, b) =>
            {
                int na = 0, nb = 0;
                int.TryParse(new string(a.Where(char.IsDigit).ToArray()), out na);
                int.TryParse(new string(b.Where(char.IsDigit).ToArray()), out nb);
                return na.CompareTo(nb);
            });
            foreach (string p in ports)
                cmbPort.Items.Add(p);
            if (cmbPort.Items.Count > 0) cmbPort.SelectedIndex = 0;
        }

        private void BtnRefresh_Click(object sender, RoutedEventArgs e) => RefreshPorts();

        private void CmbChannel_SelectionChanged(object sender, SelectionChangedEventArgs e) => UpdateChannelUi();

        private void UpdateChannelUi()
        {
            int ch = cmbChannel.SelectedIndex;
            bool needPort = (ch == 0 || ch == 1);          /* 串口 / USB-CDC */
            bool needIp   = (ch == 2);                      /* HTTP */
            cmbPort.IsEnabled = needPort;
            txtIp.IsEnabled   = needIp;
            btnStart.IsEnabled = !_running;
            txtFile.IsEnabled = !_running;
        }

        private void BtnBrowse_Click(object sender, RoutedEventArgs e)
        {
            OpenFileDialog of = new OpenFileDialog();
            of.Filter = LangResouorce.GetText("Ota_filter_otapkg");
            if (of.ShowDialog() == true)
            {
                txtFile.Text = of.FileName;
                try
                {
                    byte[] b = File.ReadAllBytes(of.FileName);
                    if (b.Length >= 8 && b[0] == 'O' && b[1] == 'T' && b[2] == 'A' && b[3] == '1')
                    {
                        int v = b[4] | (b[5] << 8) | (b[6] << 16) | (b[7] << 24);
                        string vs = "0x" + v.ToString("X8");
                        Dispatcher.BeginInvoke(new Action(() => { lblPkgVer.Content = vs; }));
                        Log(LangResouorce.GetText("Ota_log_pkgver") + vs);
                    }
                    else Log(LangResouorce.GetText("Ota_log_notota"));
                }
                catch (Exception ex) { Log(LangResouorce.GetText("Ota_log_readverfail") + ex.Message); }
            }
        }

        private void BtnReadVer_Click(object sender, RoutedEventArgs e)
        {
            int ch = cmbChannel.SelectedIndex;
            string port = cmbPort.Text != null ? cmbPort.Text.Trim() : "";
            if (ch > 1) { Log(LangResouorce.GetText("Ota_log_readver_serialonly")); return; }
            if (string.IsNullOrEmpty(port)) { Log(LangResouorce.GetText("Ota_log_selectport")); return; }
            Log(LangResouorce.GetText("Ota_log_readdev") + (ch == 0 ? LangResouorce.GetText("Ota_log_serial") : LangResouorce.GetText("Ota_log_usbcdc")) + " " + port + ")...");
            Thread t = new Thread(() =>
            {
                try
                {
                    OtaUpdater u = new OtaUpdater();
                    u.StatusChanged += s => Log(s);
                    int ver = u.QueryVersion(port, 115200, ch == 1);
                    if (ver > 0)
                    {
                        string vs = "0x" + ver.ToString("X8");
                        Dispatcher.BeginInvoke(new Action(() => { lblDevVer.Content = vs; }));
                        Log(LangResouorce.GetText("Ota_log_devver") + vs);
                    }
                    else Log(LangResouorce.GetText("Ota_log_noreply"));
                }
                catch (Exception ex) { Log(LangResouorce.GetText("Ota_log_readex") + ex.Message); }
            });
            t.IsBackground = true;
            t.Start();
        }

        private void BtnClearLog_Click(object sender, RoutedEventArgs e)
        {
            txtLog.Clear();
        }

        private void Log(string s)
        {
            Dispatcher.BeginInvoke(new Action(() =>
            {
                /* 墙钟 + 相对耗时（每次 OTA 开始清零，见 BtnStart_Click 中 _sw.Restart()） */
                txtLog.AppendText(DateTime.Now.ToString("HH:mm:ss ") + string.Format("[{0,10:F3} ms] {1}", _sw.Elapsed.TotalMilliseconds, s) + Environment.NewLine);
                txtLog.ScrollToEnd();
            }));
        }

        private void SetProgress(int pct)
        {
            Dispatcher.BeginInvoke(new Action(() =>
            {
                pbProgress.Value = pct;
                lblPercent.Content = pct + "%";
            }));
        }

        private void BtnStart_Click(object sender, RoutedEventArgs e)
        {
            /* 在 UI 线程先取好所有控件值，后台线程不得访问 UI 对象 */
            int ch = cmbChannel.SelectedIndex;
            string pkgPath = txtFile.Text.Trim();
            string port = cmbPort.Text != null ? cmbPort.Text.Trim() : "";
            string ip = txtIp.Text.Trim();
            if (string.IsNullOrEmpty(pkgPath) || !File.Exists(pkgPath))
            {
                MessageBox.Show(LangResouorce.GetText("Ota_msg_nopkg"), LangResouorce.GetText("DevSearch_msg_title"));
                return;
            }
            if (ch != 3 && string.IsNullOrEmpty(port))
            {
                MessageBox.Show(LangResouorce.GetText("Ota_msg_noport"), LangResouorce.GetText("DevSearch_msg_title"));
                return;
            }

            _running = true;
            _sw.Restart();          /* 本次 OTA 耗时起点（每行 [x ms] 相对此清零点） */
            btnStart.IsEnabled = false;
            btnStop.IsEnabled = true;      /* 运行中可随时停止，卡死可一键退出 */
            pbProgress.Value = 0;
            lblPercent.Content = "0%";

            Thread t = new Thread(() =>
            {
                try
                {
                    byte[] pkg = File.ReadAllBytes(pkgPath);
                    if (pkg.Length < OtaProtocol.PkgHdrLen)
                    {
                        Log(LangResouorce.GetText("Ota_log_badpkg"));
                        return;
                    }
                    switch (ch)
                    {
                        case 0: RunSerial(pkg, port, 115200, false); break;
                        case 1: RunSerial(pkg, port, 115200, true);  break;
                        case 2: RunHttp(pkg, ip); break;
                        case 3: Log(LangResouorce.GetText("Ota_log_usbdisk_hint")); break;
                        case 4: RunWinUsb(pkg); break;   /* v1.10: WinUSB 快通道（绕过 usbser.sys 265ms） */
                    }
                }
                catch (Exception ex)
                {
                    /* v9.82d: 打印内部异常（SocketException）定位故障模式：
                     * 拒绝连接=设备在线但 8081 未监听 / 超时=设备不在网或 IP 错 / 不可达=网络不通 */
                    string inner = "";
                    if (ex.InnerException != null)
                        inner = " → " + ex.InnerException.Message + " (" + ex.InnerException.GetType().Name + ")";
                    Log(LangResouorce.GetText("Ota_log_exception") + ex.Message + inner);
                }
                finally
                {
                    _running = false;
                    Dispatcher.BeginInvoke(new Action(() =>
                    {
                        btnStart.IsEnabled = true;
                        btnStop.IsEnabled = false;
                    }));
                    /* 升级流程结束：先让 100% 可见（设备验签/重启期间），随后自动清零复位待下次使用 */
                    Thread.Sleep(2000);
                    Dispatcher.BeginInvoke(new Action(() =>
                    {
                        pbProgress.Value = 0;
                        lblPercent.Content = "0%";
                    }));
                }
            });
            t.IsBackground = true;
            t.Start();
        }

        private void RunSerial(byte[] pkg, string port, int baud, bool isUsb)
        {
            _updater = new OtaUpdater();
            _updater.ProgressChanged += (off, total) =>
            {
                int pct = total > 0 ? off * 100 / total : 0;
                SetProgress(pct);
            };
            _updater.StatusChanged += s => Log(s);
            Log(LangResouorce.GetText("Ota_log_open") + port + " (" + (isUsb ? LangResouorce.GetText("Ota_log_usbcdc") : "115200") + ")...");
            if (!_updater.Open(port, baud, isUsb))
            {
                Log(LangResouorce.GetText("Ota_log_openfail"));
                return;
            }
            bool ok = _updater.Update(pkg);
            _updater.Close();
            Log(ok ? LangResouorce.GetText("Ota_log_done") : LangResouorce.GetText("Ota_log_failed"));
        }

        /* v1.10: WinUSB 快通道——OpenWinUsb() 打开 VID 2E88/PID 4608，绕过 usbser.sys 265ms */
        private void RunWinUsb(byte[] pkg)
        {
            _updater = new OtaUpdater();
            _updater.ProgressChanged += (off, total) =>
            {
                int pct = total > 0 ? off * 100 / total : 0;
                SetProgress(pct);
            };
            _updater.StatusChanged += s => Log(s);
            Log("opening WinUSB (VID 2E88/PID 4608)...");
            if (!_updater.OpenWinUsb())
            {
                Log("WinUSB 打开失败（检查设备 hw_inf=7 WinUSB 模式 / winusb 驱动）");
                return;
            }
            /* v1.36: 回环测试已注释（3s 纯开销；R=16ms 后无需对照验证） */
            // Log("echo test: " + _updater.WinUsbEchoTest(16, 512));
            bool ok = _updater.Update(pkg);
            _updater.Close();
            Log(ok ? LangResouorce.GetText("Ota_log_done") : LangResouorce.GetText("Ota_log_failed"));
        }

        private void RunHttp(byte[] pkg, string ip)
        {
            string url = "http://" + ip + ":8081/";
            Log(LangResouorce.GetText("Ota_log_httppush") + url + " ...");
            using (HttpClient client = new HttpClient())
            {
                client.Timeout = TimeSpan.FromSeconds(120);  /* OTA 大包 + 设备擦写耗时，120s 上限 */
                client.MaxResponseContentBufferSize = 1024 * 1024;
                /* 上传进度流：设备边收边写暂存（最耗时），进度条随上传实时更新 */
                ProgressStream ps = new ProgressStream(pkg, pct => SetProgress(pct));
                StreamContent content = new StreamContent(ps);
                content.Headers.ContentType = new System.Net.Http.Headers.MediaTypeHeaderValue("application/octet-stream");
                HttpResponseMessage resp = client.PostAsync(url, content).GetAwaiter().GetResult();
                Log(LangResouorce.GetText("Ota_log_httpresp") + (int)resp.StatusCode + " " + resp.StatusCode);
                if (resp.IsSuccessStatusCode)
                    Log(LangResouorce.GetText("Ota_log_pushdone"));
                else
                    Log(LangResouorce.GetText("Ota_log_pushfail"));
            }
        }

        private void BtnStop_Click(object sender, RoutedEventArgs e)
        {
            if (_updater != null)
            {
                try { _updater.Stop(); } catch { }
            }
            Log(LangResouorce.GetText("Ota_log_stopreq"));
        }
    }

    /// <summary>上传进度流：包装字节数组，读取（发送）时上报百分比</summary>
    internal class ProgressStream : System.IO.Stream
    {
        private readonly byte[] _data;
        private long _pos;
        private readonly Action<int> _onProgress;

        public ProgressStream(byte[] data, Action<int> onProgress) { _data = data; _onProgress = onProgress; }

        public override bool CanRead { get { return true; } }
        public override bool CanSeek { get { return true; } }   /* 可定位 → HttpClient 拿到 Content-Length 流式发送，不预缓冲 → 进度反映真实传输 */
        public override bool CanWrite { get { return false; } }
        public override long Length { get { return _data.Length; } }
        public override long Position { get { return _pos; } set { _pos = value; } }

        public override int Read(byte[] buffer, int offset, int count)
        {
            int n = Math.Min(count, _data.Length - (int)_pos);
            if (n <= 0) { _onProgress(100); return 0; }
            Buffer.BlockCopy(_data, (int)_pos, buffer, offset, n);
            _pos += n;
            _onProgress((int)(_pos * 100 / _data.Length));
            return n;
        }
        public override void Flush() { }
        public override long Seek(long offset, System.IO.SeekOrigin origin)
        {
            if (origin == System.IO.SeekOrigin.Begin) _pos = offset;
            else if (origin == System.IO.SeekOrigin.Current) _pos += offset;
            else _pos = _data.Length + offset;
            return _pos;
        }
        public override void SetLength(long value) { throw new NotSupportedException(); }
        public override void Write(byte[] buffer, int offset, int count) { throw new NotSupportedException(); }
    }
}
