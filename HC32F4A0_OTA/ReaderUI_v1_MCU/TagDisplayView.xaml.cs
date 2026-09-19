using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading;
using System.Windows;
using System.Windows.Controls;
using ReaderManager.Models;

namespace ReaderManager
{
    /// <summary>
    /// 标签显示页：接收设备端通过 串口 / USB-CDC / WinUSB / HTTP-TCP 上传的标签，按行显示。
    /// 页面框架先行，各通道协议接收实现后续接入（见 OpenChannel/CloseChannel/OnTagLine）。
    /// </summary>
    public partial class TagDisplayView : UserControl
    {
        /// <summary>单行标签数据（ListView 列绑定）</summary>
        public class TagDisplayItem : INotifyPropertyChanged
        {
            int reads_;
            public string No { get; set; }
            public string Epc { get; set; }
            public int Reads
            {
                get { return reads_; }
                set { if (value != reads_) { reads_ = value; Raise("Reads"); } }
            }
            public int Ant { get; set; }
            public string BankData { get; set; }
            public string Prot { get; set; }
            public int Rssi { get; set; }
            public int Freq { get; set; }
            public string Phase { get; set; }
            public string Time { get; set; }

            public event PropertyChangedEventHandler PropertyChanged;
            void Raise(string n) { if (PropertyChanged != null) PropertyChanged(this, new PropertyChangedEventArgs(n)); }
        }

        public ObservableCollection<TagDisplayItem> TagList { get; private set; }

        /// <summary>接口枚举：0=串口 1=USB-CDC 2=WinUSB 3=HTTP/TCP（与 cbbChannel 顺序一致）</summary>
        public enum TagChannel { Serial = 0, UsbCdc = 1, WinUsb = 2, HttpTcp = 3 }

        private volatile bool _running = false;
        private int _seq = 0;          /* 行序号 */
        private int _totalCount = 0;   /* 累计接收标签数 */
        private readonly object _lock = new object();

        public TagDisplayView()
        {
            InitializeComponent();
            TagList = new ObservableCollection<TagDisplayItem>();
            listTags.ItemsSource = TagList;
            cbbChannel.SelectedIndex = 0;   /* 默认串口 */
        }

        private void UserControl_Loaded(object sender, RoutedEventArgs e)
        {
            UpdateAddrHint();
        }

        private void UserControl_Unloaded(object sender, RoutedEventArgs e)
        {
            StopReceive();
        }

        /* ---------------- 接口选择 ---------------- */

        private void cbbChannel_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            UpdateAddrHint();
        }

        /// <summary>随接口切换更新地址/端口输入可用性</summary>
        private void UpdateAddrHint()
        {
            int ch = cbbChannel.SelectedIndex;
            if (ch == (int)TagChannel.HttpTcp)
            {
                // HTTP/TCP：地址=IP，端口=port
                tbxPort.IsEnabled = true;
            }
            else
            {
                // 串口/CDC/WinUSB：地址=COMx / USB 口，端口不适用
                tbxPort.IsEnabled = false;
                tbxPort.Text = "";
            }
        }

        /* ---------------- 控制按钮 ---------------- */

        private void btnStart_Click(object sender, RoutedEventArgs e)
        {
            if (_running) return;
            _running = true;
            btnStart.IsEnabled = false;
            btnStop.IsEnabled = true;
            lblState.Content = "● " + GetChannelName();
            OpenChannel((TagChannel)cbbChannel.SelectedIndex);
        }

        private void btnStop_Click(object sender, RoutedEventArgs e)
        {
            StopReceive();
        }

        private void btnClear_Click(object sender, RoutedEventArgs e)
        {
            lock (_lock)
            {
                TagList.Clear();
                _seq = 0;
                _totalCount = 0;
                lblCount.Content = "0";
            }
        }

        private void StopReceive()
        {
            if (!_running) return;
            _running = false;
            CloseChannel();
            btnStart.IsEnabled = true;
            btnStop.IsEnabled = false;
            lblState.Content = LangResouorce.GetText("TagDisplay_state_idle");
        }

        private string GetChannelName()
        {
            return cbbChannel.SelectedIndex == 0 ? LangResouorce.GetText("TagDisplay_ch_serial") :
                   cbbChannel.SelectedIndex == 1 ? LangResouorce.GetText("TagDisplay_ch_cdc") :
                   cbbChannel.SelectedIndex == 2 ? LangResouorce.GetText("TagDisplay_ch_winusb") :
                   LangResouorce.GetText("TagDisplay_ch_httptcp");
        }

        /* ============================================================
         * 协议接收接口（页面框架先行，实现后续接入）
         *
         * 约定：串口/CDC/WinUSB 通道收到一帧标签上报（一行），或 HTTP-TCP
         * 收到标签 JSON（{"epc":"...","rssi":..,"ant":..,...}），解析后调用
         * OnTagLine() 追加一行。列字段可选，缺省用默认值。
         * ============================================================ */

        private void OpenChannel(TagChannel ch)
        {
            // TODO: 按 ch 打开对应通道：
            //   Serial  : new SerialPort(addr, 115200) + DataReceived -> 解析 -> OnTagLine
            //   UsbCdc  : 复用 OtaUpdater/WinUsbChannel 的 CDC 读路径或 SerialPort("COM..")
            //   WinUsb  : 复用 WinUsbChannel（VID 2E88/PID 4608）后台读 -> 解析 -> OnTagLine
            //   HttpTcp : TcpClient 连接 addr:port，流式读取标签 JSON -> 解析 -> OnTagLine
        }

        private void CloseChannel()
        {
            // TODO: 关闭 OpenChannel 打开的串口/USB/TCP 连接
        }

        /// <summary>UI 线程安全：追加一行标签（协议层解析完调用）</summary>
        protected void OnTagLine(string epc, int reads, int ant, string bankData,
                                 string prot, int rssi, int freq, string phase)
        {
            Dispatcher.BeginInvoke(new Action(() =>
            {
                lock (_lock)
                {
                    TagList.Add(new TagDisplayItem
                    {
                        No = (++_seq).ToString(),
                        Epc = epc ?? "",
                        Reads = reads,
                        Ant = ant,
                        BankData = bankData ?? "",
                        Prot = prot ?? "Gen2",
                        Rssi = rssi,
                        Freq = freq,
                        Phase = phase ?? "",
                        Time = DateTime.Now.ToString("HH:mm:ss.fff")
                    });
                    _totalCount++;
                    lblCount.Content = _totalCount.ToString();
                    listTags.ScrollIntoView(TagList[TagList.Count - 1]);
                }
            }));
        }
    }
}
