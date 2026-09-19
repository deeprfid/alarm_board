using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.Net;
using System.Net.Sockets;
using System.Diagnostics;
using System.Threading;
using System.Net.NetworkInformation;

namespace broadcastConfig
{
    public partial class DevDetails : Form
    {
        public DevDetails(Form1 mf, DevInfo dev)
        {
            InitializeComponent();
            labbtype.Text = dev.BoardType;
            labmtype.Text = dev.ModType;
            labbver.Text = dev.Bver;
            labmver.Text = dev.Mver;
            labwmode.Text = dev.Wmode;
            labcstate.Text = dev.StateCode.ToString();
            cbdhcp.Checked = dev.IsDhcp;
            tbip.Text = dev.Ip;
            tbnm.Text = dev.NetMask;
            tbdns.Text = dev.Dns;
            tbport.Text = dev.LisPort.ToString();
            tbgw.Text = dev.Gateway;
            Reader = dev;
            MainFrm = mf;
        }
        DevInfo Reader;
        Form1 MainFrm;
        public static int FromU32(byte[] bytes, int offset, UInt32 value)
        {
            int end = offset;
            bytes[end++] = (byte)((value >> 24) & 0xFF);
            bytes[end++] = (byte)((value >> 16) & 0xFF);
            bytes[end++] = (byte)((value >> 8) & 0xFF);
            bytes[end++] = (byte)((value >> 0) & 0xFF);
            return end - offset;
        }
        public static byte[] EncodeU32(UInt32 value)
        {
            byte[] bytes = new byte[4];
            FromU32(bytes, 0, value);
            return bytes;
        }
        public static byte[] IPtoBytes(string ip)
        {
            uint ip_ = IPtoUint(ip);
            if (ip_ == 0)
                return null;
            else
                return EncodeU32(ip_);
        }

        private static uint IPtoUint(string ip)
        {
            char[] dep = new char[1];
            dep[0] = '.';
            //         string[] substrs = ip.Split(dep, StringSplitOptions.None);
            string[] substrs = ip.Split(dep);
            uint ret = 0;
            if (substrs.Length != 4)
                return 0;
            for (int j = 0; j < 4; ++j)
            {
                int num;
                try
                {
                    num = int.Parse(substrs[j]);
                }
                catch (Exception)
                {
                    return 0;
                }
                if (num < 0 || num > 255)
                    return 0;

            }

            for (int i = 0; i < substrs.Length; ++i)
                ret |= (uint)((byte.Parse(substrs[i])) << ((substrs.Length - i - 1) * 8));

            return ret;
        }
        public static byte[] FromHex(string hex)
        {
            byte[] bytes = new byte[hex.Length / 2];
            for (int i = 0; i < hex.Length; i += 2)
            {
                bytes[i / 2] = Byte.Parse(hex.Substring(i, 2), System.Globalization.NumberStyles.AllowHexSpecifier);
            }
            return bytes;
        }
        private void btnsetnet_Click(object sender, EventArgs e)
        {
            byte[] macb = null;
            if (cbmacaddr.Checked)
            {
                if (this.tbmacaddr.Text.Trim().Length != 12)
                {
                    MessageBox.Show("MAC地址格式错误");
                    return;
                }
                macb = FromHex(this.tbmacaddr.Text.Trim());
            }
            else
            {
                DateTime twok = new DateTime(2000, 1, 1);
                TimeSpan t = DateTime.Now - twok;
                uint mac = (uint)(t.Ticks / 10000000);
                macb = new byte[6];

                macb[0] = 0xfc;
                macb[1] = 0xff;
                macb[2] = (byte)((mac >> 24) & 0xff);
                macb[3] = (byte)((mac >> 16) & 0xff);
                macb[4] = (byte)((mac >> 8) & 0xff);
                macb[5] = (byte)((mac >> 0) & 0xff);
            }

            List<byte> cmd = new List<byte>();
            cmd.AddRange(Reader.MacBytes);
            if (cbdhcp.Checked)
            {
                cmd.AddRange(new byte[] { 0x00, 0x00, 0x00, 0x00 });
                cmd.AddRange(new byte[] { 0x00, 0x00, 0x00, 0x00 });
                cmd.AddRange(new byte[] { 0x00, 0x00, 0x00, 0x00 });
                cmd.AddRange(new byte[] { 0x00, 0x00, 0x00, 0x00 });
            }
            else
            {
                if (this.tbip.Text.Trim() == string.Empty || this.tbnm.Text.Trim() == string.Empty
                    || this.tbgw.Text.Trim() == string.Empty || this.tbdns.Text.Trim() == string.Empty)
                {
                    MessageBox.Show("请输入ip地址相关项");
                    return;
                }
                byte[] bytes_ = IPtoBytes(tbip.Text);
                if (bytes_ == null)
                {
                    MessageBox.Show("ip地址输入错误");
                    return;
                }
                cmd.AddRange(bytes_);

                bytes_ = IPtoBytes(tbnm.Text);
                if (bytes_ == null)
                {
                    MessageBox.Show("子网掩码输入错误");
                    return;
                }
                cmd.AddRange(bytes_);

                bytes_ = IPtoBytes(tbgw.Text);
                if (bytes_ == null)
                {
                    MessageBox.Show("网关输入错误");
                    return;
                }
                cmd.AddRange(bytes_);

                bytes_ = IPtoBytes(tbdns.Text);
                if (bytes_ == null)
                {
                    MessageBox.Show("网关输入错误");
                    return;
                }
                cmd.AddRange(bytes_);

            }

            cmd.AddRange(macb);

            try
            {
                ushort lport = ushort.Parse(tbport.Text);
                cmd.Add((byte)((lport >> 8) & 0xff));
                cmd.Add((byte)((lport >> 0) & 0xff));
            }
            catch
            {
                MessageBox.Show("绑定端口输入错误");
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

            UdpClient UDPClient = new UdpClient(new IPEndPoint(IPAddress.Parse(MainFrm.localIps[MainFrm.localIpsIndex]), 0));
            IPEndPoint endpoint = new IPEndPoint(IPAddress.Broadcast, 15000);
            UDPClient.EnableBroadcast = true;

            IPEndPoint remote = new IPEndPoint(IPAddress.Any, 0);

            byte[] buf = cmdfull.ToArray();
            UDPClient.Send(buf, buf.Length, endpoint);

            UDPClient.Client.ReceiveTimeout = 2000;

            try
            {
                byte[] resp = UDPClient.Receive(ref remote);
                string msg = "";
                for (int i = 0; i < resp.Length; ++i)
                    msg += resp[i].ToString("X2") + " ";
                Debug.WriteLine("resp:" + msg);
                MessageBox.Show("设置成功");
            }
            catch
            {
                MessageBox.Show("设置失败");
            }
        }

        private void cbdhcp_CheckedChanged(object sender, EventArgs e)
        {
            if (cbdhcp.Checked)
            {
                tbdns.Enabled = false;
                tbip.Enabled = false;
                tbgw.Enabled = false;
                tbnm.Enabled = false;
            }
            else
            {
                tbdns.Enabled = true;
                tbip.Enabled = true;
                tbgw.Enabled = true;
                tbnm.Enabled = true;
            }
        }

        private void cbmacaddr_CheckedChanged(object sender, EventArgs e)
        {
            if (cbmacaddr.Checked)
                tbmacaddr.Enabled = true;
            else
                tbmacaddr.Enabled = false;
        }

        /*
         * 格式:0xEE, 0x06(表示读写器名字，在广播指令中是读写器的mac地址，所以固定为6个字节)，
         * 数据段长度（2字节），命令码（1字节），指令标志位（1字节，固定为01，表示必须回复），
         * 读写器mac地址（6字节，全0表示如何读写器都执行，否则表示只有相同mac地址读写器执行），
         * 数据段（变长）
         * */
        private void btnback2passive_Click(object sender, EventArgs e)
        {

            List<byte> cmd = new List<byte>();
            cmd.Add(0xEE);
            cmd.Add(0x06);

            cmd.Add(0x00);
            cmd.Add(0x00);

            cmd.Add(0x53);
            cmd.Add(0x01);

            cmd.AddRange(Reader.MacBytes);

            UdpClient UDPClient = new UdpClient(new IPEndPoint(IPAddress.Parse(MainFrm.localIps[MainFrm.localIpsIndex]), 0));
            IPEndPoint endpoint = new IPEndPoint(IPAddress.Broadcast, 15000);
            UDPClient.EnableBroadcast = true;

            IPEndPoint remote = new IPEndPoint(IPAddress.Any, 0);

            byte[] buf = cmd.ToArray();
            UDPClient.Send(buf, buf.Length, endpoint);

            UDPClient.Client.ReceiveTimeout = 2000;
            try
            {
                byte[] resp = UDPClient.Receive(ref remote);
                string msg = "";
                for (int i = 0; i < resp.Length; ++i)
                    msg += resp[i].ToString("X2") + " ";
                Debug.WriteLine("resp:" + msg);
                MessageBox.Show("设置成功");
            }
            catch
            {
                MessageBox.Show("设置失败");
            }
        }


    }
}
