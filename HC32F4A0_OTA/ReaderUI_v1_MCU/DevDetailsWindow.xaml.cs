using ReaderManager.Models;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Windows;

namespace ReaderManager
{
    public partial class DevDetailsWindow : Window
    {
        private DevInfo _dev;
        private string _localIp;

        public DevDetailsWindow(DevInfo dev, string localIp)
        {
            InitializeComponent();
            _dev = dev;
            _localIp = localIp;
            LoadDevInfo();
        }

        private void LoadDevInfo()
        {
            lblBoardType.Text = _dev.BoardType ?? "-";
            lblModType.Text = _dev.ModType ?? "-";
            lblIp.Text = _dev.Ip ?? "-";
            lblBVer.Text = TranslateValue(_dev.Bver) ?? "-";
            lblMVer.Text = TranslateValue(_dev.Mver) ?? "-";
            lblMac.Text = _dev.Mac ?? "-";
            lblWorkMode.Text = TranslateValue(_dev.Wmode) ?? "-";
            lblStateCode.Text = _dev.StateCode.ToString();

            txtIp.Text = _dev.Ip;
            txtNetMask.Text = _dev.NetMask;
            txtGateway.Text = _dev.Gateway;
            txtDns.Text = _dev.Dns;
            txtPort.Text = _dev.LisPort.ToString();
            chkDhcp.IsChecked = _dev.IsDhcp;
        }

        /// <summary>
        /// 枚举字段语言映射：搜索时固化的中文（未知/被动/主动）在显示时按当前语言翻译。
        /// 固化为英文时原样返回（英文本身无需翻译）。
        /// </summary>
        private static string TranslateValue(string v)
        {
            if (v == null) return null;
            switch (v)
            {
                case "未知": return LangResouorce.GetText("DevSearch_unknown");
                case "被动": return LangResouorce.GetText("DevSearch_passive");
                case "主动": return LangResouorce.GetText("DevSearch_active");
                default: return v;
            }
        }

        private void chkDhcp_CheckedChanged(object sender, RoutedEventArgs e)
        {
            bool isDhcp = chkDhcp.IsChecked == true;
            txtIp.IsEnabled = !isDhcp;
            txtNetMask.IsEnabled = !isDhcp;
            txtGateway.IsEnabled = !isDhcp;
            txtDns.IsEnabled = !isDhcp;
        }

        private void chkMacAddr_CheckedChanged(object sender, RoutedEventArgs e)
        {
            txtMacAddr.IsEnabled = chkMacAddr.IsChecked == true;
        }

        private async void btnSetNet_Click(object sender, RoutedEventArgs e)
        {
            byte[] macBytes = _dev.MacBytes;

            if (chkMacAddr.IsChecked == true)
            {
                if (txtMacAddr.Text.Trim().Length != 12)
                {
                    MessageBox.Show(LangResouorce.GetText("DevDetails_msg_macerr"));
                    return;
                }
                macBytes = FromHex(txtMacAddr.Text.Trim());
            }
            else
            {
                DateTime twok = new DateTime(2000, 1, 1);
                TimeSpan t = DateTime.Now - twok;
                uint mac = (uint)(t.Ticks / 10000000);
                macBytes = new byte[6];
                macBytes[0] = 0xfc;
                macBytes[1] = 0xff;
                macBytes[2] = (byte)((mac >> 24) & 0xff);
                macBytes[3] = (byte)((mac >> 16) & 0xff);
                macBytes[4] = (byte)((mac >> 8) & 0xff);
                macBytes[5] = (byte)((mac >> 0) & 0xff);
            }

            List<byte> cmd = new List<byte>();
            cmd.AddRange(_dev.MacBytes);

            if (chkDhcp.IsChecked == true)
            {
                cmd.AddRange(new byte[] { 0x00, 0x00, 0x00, 0x00 });
                cmd.AddRange(new byte[] { 0x00, 0x00, 0x00, 0x00 });
                cmd.AddRange(new byte[] { 0x00, 0x00, 0x00, 0x00 });
                cmd.AddRange(new byte[] { 0x00, 0x00, 0x00, 0x00 });
            }
            else
            {
                if (string.IsNullOrEmpty(txtIp.Text.Trim()) || string.IsNullOrEmpty(txtNetMask.Text.Trim())
                    || string.IsNullOrEmpty(txtGateway.Text.Trim()) || string.IsNullOrEmpty(txtDns.Text.Trim()))
                {
                    MessageBox.Show(LangResouorce.GetText("DevDetails_msg_ipempty"));
                    return;
                }

                var ipBytes = IPtoBytes(txtIp.Text.Trim());
                if (ipBytes == null) { MessageBox.Show(LangResouorce.GetText("DevDetails_msg_iperr")); return; }
                cmd.AddRange(ipBytes);

                ipBytes = IPtoBytes(txtNetMask.Text.Trim());
                if (ipBytes == null) { MessageBox.Show(LangResouorce.GetText("DevDetails_msg_maskerr")); return; }
                cmd.AddRange(ipBytes);

                ipBytes = IPtoBytes(txtGateway.Text.Trim());
                if (ipBytes == null) { MessageBox.Show(LangResouorce.GetText("DevDetails_msg_gwerr")); return; }
                cmd.AddRange(ipBytes);

                ipBytes = IPtoBytes(txtDns.Text.Trim());
                if (ipBytes == null) { MessageBox.Show(LangResouorce.GetText("DevDetails_msg_dnserr")); return; }
                cmd.AddRange(ipBytes);
            }

            cmd.AddRange(macBytes);

            try
            {
                ushort lport = ushort.Parse(txtPort.Text.Trim());
                cmd.Add((byte)((lport >> 8) & 0xff));
                cmd.Add((byte)((lport >> 0) & 0xff));
            }
            catch
            {
                MessageBox.Show(LangResouorce.GetText("DevDetails_msg_porterr"));
                return;
            }

            List<byte> cmdfull = new List<byte>();
            cmdfull.Add(0xEE);
            cmdfull.Add(0x06);
            cmdfull.Add((byte)(((cmd.Count - 6) >> 8) & 0xff));
            cmdfull.Add((byte)(((cmd.Count - 6) >> 0) & 0xff));
            cmdfull.Add(0x51);
            cmdfull.Add(0x01);
            cmdfull.AddRange(cmd);

            bool ok = await SendUdpCommand(cmdfull.ToArray());
            lblSetResult.Text = ok ? LangResouorce.GetText("DevDetails_set_ok") : LangResouorce.GetText("DevDetails_set_fail");
        }

        private async void btnBackToPassive_Click(object sender, RoutedEventArgs e)
        {
            List<byte> cmd = new List<byte>();
            cmd.Add(0xEE);
            cmd.Add(0x06);
            cmd.Add(0x00);
            cmd.Add(0x00);
            cmd.Add(0x53);
            cmd.Add(0x01);
            cmd.AddRange(_dev.MacBytes);

            bool ok = await SendUdpCommand(cmd.ToArray());
            MessageBox.Show(ok ? LangResouorce.GetText("DevDetails_switch_ok") : LangResouorce.GetText("DevDetails_switch_fail"));
        }

        private async System.Threading.Tasks.Task<bool> SendUdpCommand(byte[] buf)
        {
            try
            {
                UdpClient client = new UdpClient(new IPEndPoint(IPAddress.Parse(_localIp), 0));
                client.EnableBroadcast = true;
                IPEndPoint endpoint = new IPEndPoint(IPAddress.Broadcast, 15000);
                await client.SendAsync(buf, buf.Length, endpoint);
                client.Client.ReceiveTimeout = 2000;
                IPEndPoint remote = new IPEndPoint(IPAddress.Any, 0);
                var result = await client.ReceiveAsync();
                client.Close();
                return true;
            }
            catch (Exception ex)
            {
                Debug.WriteLine("UDP send error: " + ex.Message);
                return false;
            }
        }

        private static byte[] IPtoBytes(string ip)
        {
            string[] parts = ip.Split('.');
            if (parts.Length != 4) return null;
            byte[] bytes = new byte[4];
            for (int i = 0; i < 4; i++)
            {
                int v;
                if (!int.TryParse(parts[i], out v) || v < 0 || v > 255)
                    return null;
                bytes[i] = (byte)v;
            }
            return bytes;
        }

        private static byte[] FromHex(string hex)
        {
            byte[] bytes = new byte[hex.Length / 2];
            for (int i = 0; i < hex.Length; i += 2)
                bytes[i / 2] = byte.Parse(hex.Substring(i, 2), System.Globalization.NumberStyles.AllowHexSpecifier);
            return bytes;
        }
    }
}