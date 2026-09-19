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
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();

            string name = Dns.GetHostName();
            IPAddress[] ipadrlist = Dns.GetHostAddresses(name);
            foreach (IPAddress ipa in ipadrlist)
            {
                if (ipa.AddressFamily == AddressFamily.InterNetwork)
                {
                    Debug.WriteLine(ipa.ToString());
                    localIps.Add(ipa.ToString());
                    cbbIps.Items.Add(ipa.ToString());
                }
            }
            
            lvDevs.Columns[0].Width = 50;
            lvDevs.Columns[1].Width = 110;
            lvDevs.Columns[2].Width = 130;
            lvDevs.Columns[3].Width = 60;
            lvDevs.Columns[4].Width = 60;
            lvDevs.Columns[5].Width = 85;
            lvDevs.Columns[6].Width = 85;
        }
        public List<string> localIps = new List<string>();
        public int localIpsIndex;
        UdpClient UDPClient = null;

        bool IsSearch = false;

        private const byte MODEL_M5E = 0x00;
        private const byte MODEL_M5E_COMPACT = 0x01;
        private const byte MODEL_M5E_PRC = 0x02;
        private const byte MODEL_M4E = 0x03;
        private const byte MODEL_M6E = 0x18;
        private const byte MODEL_M6E_PRC = 0x19;
        private const byte MODEL_M6E_MICRO = 0x20;
        private const byte MODEL_SLR1100 = 0xA0;
        private const byte MODEL_SLR1200 = 0xA1;
        private const byte MODEL_SLR3000 = 0xA2;
        private const byte MODEL_SLR5100 = 0xA3;
        private const byte MODEL_SLR5200 = 0xA4;
        private const byte MODEL_SLR3100 = 0xA5;
        private const byte MODEL_SLR5300 = 0xA6;
        private const byte MODEL_SLR3200 = 0xA7;
        private const byte MODEL_SLR5900 = 0xA9;
        private const byte MODEL_SLR5800 = 0xA8;
        private const byte MODEL_SLR6000 = 0xAA;
        private const byte MODEL_SLR6100 = 0xAB;

        private const byte MODEL_E710 = 0x31;
        private const byte MODEL_E510 = 0x32;
        private const byte MODEL_E310 = 0x33;
        private const byte MODEL_E910 = 0x34;


        Dictionary<string, DevInfo> dicDevs = new Dictionary<string, DevInfo>();
        delegate void AddDevice(DevInfo dev);

        void AddReader(DevInfo dev)
        {
            ListViewItem item = new ListViewItem(lvDevs.Items.Count.ToString());

            item.SubItems.Add(dev.Ip);
            item.SubItems.Add(dev.Mac);
            item.SubItems.Add(dev.BoardType);
            item.SubItems.Add(dev.ModType);
            item.SubItems.Add(dev.Bver);
            item.SubItems.Add(dev.Mver);


            lvDevs.Items.Add(item);
        }

        void SearchRecv()
        {
            UDPClient = new UdpClient(new IPEndPoint(IPAddress.Parse(localIps[localIpsIndex]), 0));
            IPEndPoint endpoint = new IPEndPoint(IPAddress.Broadcast, 15000);
            UDPClient.EnableBroadcast = true;

            IPEndPoint remote = new IPEndPoint(IPAddress.Any, 0);

            List<byte> cmd = new List<byte>();
            cmd.Add(0xEE);
            cmd.Add(0x06);

            cmd.Add(0x00);
            cmd.Add(0x00);

            cmd.Add(0x50);
            cmd.Add(0x01);

            cmd.Add(0x00);
            cmd.Add(0x00);
            cmd.Add(0x00);
            cmd.Add(0x00);
            cmd.Add(0x00);
            cmd.Add(0x00);

            byte[] buf = cmd.ToArray();
            UDPClient.Send(buf, buf.Length, endpoint);

            while (IsSearch)
            {
                try
                {
                    byte[] resp = UDPClient.Receive(ref remote);
                    string msg = "";
                    for (int i = 0; i < resp.Length; ++i)
                        msg += resp[i].ToString("X2")+" ";
                    Debug.WriteLine("resp:" + msg);

                    if (resp.Length < 16)
                        continue;

                    DevInfo dinfo = new DevInfo();
                    int pos = 6;

                    dinfo.StateCode = (resp[pos] << 24) | (resp[pos + 1] << 16) |
                        (resp[pos + 2] << 8) | (resp[pos + 3] << 0);
                    pos += 4;

                    dinfo.MacBytes = new byte[6];
                    Array.Copy(resp, pos, dinfo.MacBytes, 0, 6);
                    for (int i = 0; i < 6; ++i)
                        dinfo.Mac += resp[pos + i].ToString("X2")+":";
                    dinfo.Mac = dinfo.Mac.Substring(0, dinfo.Mac.Length - 1);
                    pos += 6;

                    dinfo.IsDhcp = (resp[pos++] == 0x01) ? true : false;

                    for (int i = 0; i < 4; ++i)
                        dinfo.Ip += resp[pos + i].ToString() + ".";
                    dinfo.Ip = dinfo.Ip.Substring(0, dinfo.Ip.Length - 1);
                    pos += 4;

                    for (int i = 0; i < 4; ++i)
                        dinfo.NetMask += resp[pos + i].ToString() + ".";
                    dinfo.NetMask = dinfo.NetMask.Substring(0, dinfo.NetMask.Length - 1);
                    pos += 4;

                    for (int i = 0; i < 4; ++i)
                        dinfo.Gateway += resp[pos + i].ToString() + ".";
                    dinfo.Gateway = dinfo.Gateway.Substring(0, dinfo.Gateway.Length - 1);
                    pos += 4;

                    for (int i = 0; i < 4; ++i)
                        dinfo.Dns += resp[pos + i].ToString() + ".";
                    dinfo.Dns = dinfo.Dns.Substring(0, dinfo.Dns.Length - 1);
                    pos += 4;

                    dinfo.LisPort = (resp[pos] << 8) | resp[pos+1];
                    pos += 2;

                    if (resp[pos] == 0x01)
                        dinfo.BoardType = "Hc32f46x";
                    else
                        dinfo.BoardType = "未知";
                    pos++;
                    Debug.WriteLine("modtype:" + resp[pos].ToString("X2"));
                    switch (resp[pos])
                    {
                        case MODEL_SLR1100:
                            dinfo.ModType = "SLR1100";
                            break;
                        case MODEL_SLR3000:
                            dinfo.ModType = "SLR3000";
                            break;
                        case MODEL_SLR1200:
                            dinfo.ModType = "SLR1200";
                            break;
                        case MODEL_SLR3100:
                            dinfo.ModType = "SLR3100";
                            break;
                        case MODEL_SLR5100:
                            dinfo.ModType = "SLR5100";
                            break;
                        case MODEL_SLR5200:
                            dinfo.ModType = "SLR5200";
                            break;
                        case MODEL_SLR5300:
                            dinfo.ModType = "SLR5300";
                            break;
                        case MODEL_SLR5900:
                            dinfo.ModType = "SLR5900";
                            break;
                        case MODEL_SLR5800:
                            dinfo.ModType = "SLR5800";
                            break;
                        case MODEL_SLR6000:
                            dinfo.ModType = "SLR6000";
                            break;
                        case MODEL_SLR6100:
                            dinfo.ModType = "SLR6100";
                            break;
                        case MODEL_SLR3200:
                            dinfo.ModType = "SLR3200";
                            break;
                        case MODEL_E310:
                            {
                                if (resp[pos+1] == 0)
                                    dinfo.ModType = "SIM3100";
                                else if (resp[pos + 1] == 2)
                                    dinfo.ModType = "SIM3200";
                                else if (resp[pos + 1] == 3)
                                    dinfo.ModType = "SIM3300";
                                else if (resp[pos + 1] == 4)
                                    dinfo.ModType = "SIM3400";
                                else if (resp[pos + 1] == 16)
                                    dinfo.ModType = "SIM3500";
                                else if (resp[pos + 1] == 32)
                                    dinfo.ModType = "SIM3600";
                            }
                            break;
                        case MODEL_E510:
                            {
                                if (resp[pos + 1] == 0)
                                    dinfo.ModType = "SIM5100";
                                else if (resp[pos + 1] == 2)
                                    dinfo.ModType = "SIM5200";
                                else if (resp[pos + 1] == 3)
                                    dinfo.ModType = "SIM5300";
                                else if (resp[pos + 1] == 4)
                                    dinfo.ModType = "SIM5400";
                                else if (resp[pos + 1] == 16)
                                    dinfo.ModType = "SIM5500";
                                else if (resp[pos + 1] == 32)
                                    dinfo.ModType = "SIM5600";
                            }
                            break;
                        case MODEL_E710:
                            {
                                if (resp[pos + 1] == 0)
                                    dinfo.ModType = "SIM7100";
                                else if (resp[pos + 1] == 2)
                                    dinfo.ModType = "SIM7200";
                                else if (resp[pos + 1] == 3)
                                    dinfo.ModType = "SIM7300";
                                else if (resp[pos + 1] == 4)
                                    dinfo.ModType = "SIM7400";
                                else if (resp[pos + 1] == 16)
                                    dinfo.ModType = "SIM7500";
                                else if (resp[pos + 1] == 32)
                                    dinfo.ModType = "SIM7600";
                            }
                            break;
                        case MODEL_E910:
                            dinfo.ModType = "SIM7100";
                            break;
                        default:
                            dinfo.ModType = "未知";
                            break;
                    }
                    pos += 2;


                    if (resp[pos] == 0x00 && resp[pos + 1] == 0x00 &&
                        resp[pos + 2] == 0x00 && resp[pos + 3] == 0x00)
                        dinfo.Bver = "未知";
                    else
                    {
                        for (int i = 0; i < 4; ++i)
                            dinfo.Bver += resp[pos + i].ToString() + ".";
                        dinfo.Bver = dinfo.Bver.Substring(0, dinfo.Bver.Length - 1);
                    }
                    pos += 4;

                    if (resp[pos] == 0x00 && resp[pos+1] == 0x00 && 
                        resp[pos+2] == 0x00 && resp[pos+3] == 0x00)
                        dinfo.Mver = "未知";
                    else
                    {
                        for (int i = 0; i < 4; ++i)
                            dinfo.Mver += resp[pos + i].ToString("X2") + ".";
                        dinfo.Mver = dinfo.Mver.Substring(0, dinfo.Mver.Length - 1);
                    }
                    pos += 4;
         

                    if (resp[pos] == 0x00)
                        dinfo.Wmode = "bootloader";
                    else if (resp[pos] == 0x01)
                        dinfo.Wmode = "被动";
                    else
                        dinfo.Wmode = "主动";

                    dicDevs.Add(dinfo.Mac, dinfo);
                    this.BeginInvoke(new AddDevice(AddReader), new object[] { dinfo });
                }
                catch
                {
                    Debug.WriteLine("UDPClient return ");
                }              
            }
            

        }

        private void btnSearch_Click(object sender, EventArgs e)
        {
            if (cbbIps.SelectedIndex == -1)
            {
                MessageBox.Show("请选择网络接口");
                return;
            }
            Thread receThread = new Thread(new ThreadStart(SearchRecv));
            IsSearch = true;

            receThread.Start();
            btnStart.Enabled = false;
            this.timer1.Enabled = true;
        }

        private void btnStop_Click(object sender, EventArgs e)
        {
            if (UDPClient != null)
            {
                IsSearch = false;
                UDPClient.Close();
                UDPClient = null;
            }
            btnStart.Enabled = true;
            this.timer1.Enabled = false;
        }

        private void cbbIps_SelectedIndexChanged(object sender, EventArgs e)
        {
            localIpsIndex = cbbIps.SelectedIndex;
        }

        private void Form1_FormClosing(object sender, FormClosingEventArgs e)
        {
            this.btnStop_Click(null, null);
        }

        private void timer1_Tick(object sender, EventArgs e)
        {
            this.btnStop_Click(null, null);
        }

        private void btnClear_Click(object sender, EventArgs e)
        {
            dicDevs.Clear();
            lvDevs.Items.Clear();
        }

        private void lvDevs_MouseDoubleClick(object sender, MouseEventArgs e)
        {
            DevDetails frm = new DevDetails(this, dicDevs[lvDevs.SelectedItems[0].SubItems[2].Text]);
            frm.ShowDialog();
        }
    }

    public class DevInfo
    {
        public DevInfo()
        {
            Ip = "";
            Mac = "";
            BoardType = "";
            ModType = "";
            NetMask = "";
            Gateway = "";
            Dns = "";
            Bver = "";
            Mver = "";
            Wmode = "";
        }
        public string Ip { get; set; }
        public string Mac { get; set; }
        public string BoardType { get; set; }
        public string ModType { get; set; }
        public string NetMask { get; set; }
        public string Gateway { get; set; }
        public string Dns { get; set; }
        public int LisPort { get; set; }
        public string Bver { get; set; }
        public string Mver { get; set; }
        public string Wmode { get; set; }
        public int StateCode { get; set; }
        public bool IsDhcp { get; set; }
        public byte[] MacBytes { get; set; }
    }
}
