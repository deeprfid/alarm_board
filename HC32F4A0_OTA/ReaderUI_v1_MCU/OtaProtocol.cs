using System;

namespace ReaderManager
{
    /// <summary>
    /// OTA1 帧协议（与设备端 ota_frame.c / ota_transport_uart.h 一致）
    /// 帧：[0:4]"OTA1" [4]type [5:7]seq(16LE) [7:9]len(16LE) [9:9+N]payload [末2]CRC16-CCITT-FALSE(LE)
    /// type: 0x50=DATA 0x51=ACK(设备,4B LE 已收偏移) 0x52=RESUME
    /// CRC16: poly 0x1021, init 0xFFFF, MSB-first
    /// </summary>
    public static class OtaProtocol
    {
        public const byte TypeData   = 0x50;
        public const byte TypeAck    = 0x51;
        public const byte TypeResume = 0x52;
        public const int  FrameHdrLen = 9;    /* OTA1(4)+type(1)+seq(2)+len(2) */
        public const int  FrameCrcLen = 2;
        public const int  MaxPayload  = 4096; /* 与设备帧对齐 */
        public const int  PkgHdrLen   = 82;   /* 统一 OTA 包头（OTA1+ver+plat+app+len+crc+sha+hmac） */

        public static byte[] MakeFrame(byte type, ushort seq, byte[] payload)
        {
            int plen = payload != null ? payload.Length : 0;
            byte[] f = new byte[FrameHdrLen + plen + FrameCrcLen];
            f[0] = (byte)'O'; f[1] = (byte)'T'; f[2] = (byte)'A'; f[3] = (byte)'1';
            f[4] = type;
            f[5] = (byte)(seq & 0xFF); f[6] = (byte)((seq >> 8) & 0xFF);
            f[7] = (byte)(plen & 0xFF); f[8] = (byte)((plen >> 8) & 0xFF);
            if (plen > 0) Buffer.BlockCopy(payload, 0, f, FrameHdrLen, plen);
            ushort crc = Crc16(f, FrameHdrLen + plen);
            f[FrameHdrLen + plen]     = (byte)(crc & 0xFF);
            f[FrameHdrLen + plen + 1] = (byte)((crc >> 8) & 0xFF);
            return f;
        }

        /// <summary>CRC16-CCITT-FALSE：poly 0x1021, init 0xFFFF, MSB-first（与设备 ota_transport_uart_crc16 一致）</summary>
        public static ushort Crc16(byte[] buf, int len)
        {
            ushort crc = 0xFFFF;
            for (int i = 0; i < len; i++)
            {
                crc ^= (ushort)(buf[i] << 8);
                for (int k = 0; k < 8; k++)
                    crc = (ushort)((crc & 0x8000) != 0 ? ((crc << 1) ^ 0x1021) : (crc << 1));
            }
            return crc;
        }

        /// <summary>
        /// 从缓冲解析 ACK/RESUME 帧：成功返回设备确认偏移；缓冲不足返回 -2（需继续收）；
        /// 无效帧返回 -1（丢弃重收）
        /// </summary>
        public static int TryParseAck(byte[] buf, int len, out byte type)
        {
            type = 0;
            if (len < FrameHdrLen) return -2;
            if (buf[0] != (byte)'O' || buf[1] != (byte)'T' || buf[2] != (byte)'A' || buf[3] != (byte)'1')
                return -1;
            type = buf[4];
            int plen = buf[7] | (buf[8] << 8);
            int total = FrameHdrLen + plen + FrameCrcLen;
            if (len < total) return -2;
            ushort crcGot = (ushort)(buf[total - 2] | (buf[total - 1] << 8));
            if (Crc16(buf, total - 2) != crcGot) return -1;
            if (type != TypeAck && type != TypeResume) return -1;
            if (plen < 4) return -1;
            return buf[9] | (buf[10] << 8) | (buf[11] << 16) | (buf[12] << 24);
        }
    }
}
