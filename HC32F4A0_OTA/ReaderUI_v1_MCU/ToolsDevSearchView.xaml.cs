using ReaderManager.Models;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

namespace ReaderManager
{
    public partial class ToolsDevSearchView : UserControl
    {
        private const int BroadcastPort = 15000;
        private CancellationTokenSource _cts;
        private ObservableCollection<DevInfo> _devices = new ObservableCollection<DevInfo>();
        private Dictionary<string, DevInfo> _devDict = new Dictionary<string, DevInfo>();
        private string _selectedLocalIp;

        private static readonly Dictionary<byte, string> ModTypeMap = new Dictionary<byte, string>
        {
            {0x00, "M5E"}, {0x01, "M5E-COMPACT"}, {0x02, "M5E-PRC"},
            {0x03, "M4E"}, {0x18, "M6E"}, {0x19, "M6E-PRC"}, {0x20, "M6E-MICRO"},
            {0xA0, "SLR1100"}, {0xA1, "SLR1200"}, {0xA2, "SLR3000"},
            {0xA3, "SLR5100"}, {0xA4, "SLR5200"}, {0xA5, "SLR3100"},
            {0xA6, "SLR5300"}, {0xA7, "SLR3200"}, {0xA8, "SLR5800"},
            {0xA9, "SLR5900"}, {0xAA, "SLR6000"}, {0xAB, "SLR6100"},
            {0x31, "E710"}, {0x32, "E510"}, {0x33, "E310"}, {0x34, "E910"}
        };

        public ToolsDevSearchView()
        {
            InitializeComponent();
            dgDevices.ItemsSource = _devices;
            LoadLocalIps();
            UpdateColumnHeaders();
            Loaded += (s, e) => UpdateColumnHeaders();
            var mw = ReaderParamsViewModel.GetMainWindow();
            if (mw != null) mw.AllViews["ToolsDevSearchView"] = this;
        }

        /// <summary>
        /// DataGrid 列头不在可视树，DynamicResource 不生效，必须代码设置。
        /// 语言切换后由 MainWindow 调用本方法刷新列头。
        /// </summary>
        public void UpdateColumnHeaders()
        {
            colNo.Header = LangResouorce.GetText("DevSearch_col_no");
            colIp.Header = LangResouorce.GetText("DevSearch_col_ip");
            colMac.Header = LangResouorce.GetText("DevSearch_col_mac");
            colBoardType.Header = LangResouorce.GetText("DevSearch_col_boardtype");
            colModType.Header = LangResouorce.GetText("DevSearch_col_modtype");
            colBver.Header = LangResouorce.GetText("DevSearch_col_bver");
            colMver.Header = LangResouorce.GetText("DevSearch_col_mver");
            colWmode.Header = LangResouorce.GetText("DevSearch_col_wmode");
        }

        /// <summary>
        /// 获取本机所有 IPv4 地址填充到下拉框
        /// </summary>
        private void LoadLocalIps()
        {
            try
            {
                string hostName = Dns.GetHostName();
                var addresses = Dns.GetHostAddresses(hostName)
                    .Where(ip => ip.AddressFamily == AddressFamily.InterNetwork)
                    .Select(ip => ip.ToString())
                    .ToList();

                cbbIps.ItemsSource = addresses;
                if (addresses.Count > 0)
                    cbbIps.SelectedIndex = 0;
            }
            catch (Exception ex)
            {
                Debug.WriteLine("LoadLocalIps error: " + ex.Message);
            }
        }

        private void btnSearch_Click(object sender, RoutedEventArgs e)
        {
            if (cbbIps.SelectedItem == null)
            {
                MessageBox.Show(LangResouorce.GetText("DevSearch_msg_selectif"), LangResouorce.GetText("DevSearch_msg_title"));
                return;
            }

            _selectedLocalIp = cbbIps.SelectedItem.ToString();

            // 取消之前的搜索
            _cts?.Cancel();
            _cts = new CancellationTokenSource(TimeSpan.FromSeconds(5));

            btnSearch.IsEnabled = false;
            btnStop.IsEnabled = true;
            txtStatus.Text = LangResouorce.GetText("DevSearch_status_searching");

            _devices.Clear();
            _devDict.Clear();
            Utility.ReplaceDeviceIps(new List<string>());
            Utility.RefreshAllDeviceIps();

            Task.Run(() => SearchDevices(_cts.Token));
        }

        private async Task SearchDevices(CancellationToken token)
        {
            UdpClient udpClient = null;
            try
            {
                udpClient = new UdpClient(new IPEndPoint(IPAddress.Parse(_selectedLocalIp), 0));
                udpClient.EnableBroadcast = true;

                // 发送搜索广播
                byte[] cmd = new byte[] { 0xEE, 0x06, 0x00, 0x00, 0x50, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
                IPEndPoint broadcastEp = new IPEndPoint(IPAddress.Broadcast, BroadcastPort);
                await udpClient.SendAsync(cmd, cmd.Length, broadcastEp);

                while (!token.IsCancellationRequested)
                {
                    try
                    {
                        var result = await udpClient.ReceiveAsync().WithCancellation(token);
                        byte[] resp = result.Buffer;

                        if (resp.Length < 16) continue;

                        DevInfo dev = ParseResponse(resp);
                        if (dev == null) continue;

                        // 去重
                        Application.Current.Dispatcher.Invoke(() =>
                        {
                            if (!_devDict.ContainsKey(dev.Mac))
                            {
                                dev.No = (_devices.Count + 1).ToString();
                                _devDict[dev.Mac] = dev;
                                _devices.Add(dev);

                                txtStatus.Text = string.Format(LangResouorce.GetText("DevSearch_status_found"), _devices.Count);
                            }
                        });
                    }
                    catch (ObjectDisposedException) { break; }
                    catch (OperationCanceledException) { break; }
                    catch (Exception ex)
                    {
                        Debug.WriteLine("UDP recv error: " + ex.Message);
                        break;
                    }
                }
            }
            catch (Exception ex)
            {
                Debug.WriteLine("Search error: " + ex.Message);
            }
            finally
            {
                udpClient?.Close();
                Application.Current.Dispatcher.Invoke(() =>
                {
                    btnSearch.IsEnabled = true;
                    btnStop.IsEnabled = false;
                    var onlineIps = _devices.Select(d => d.Ip).ToList();
                    Utility.ReplaceDeviceIps(onlineIps);
                    Utility.RefreshAllDeviceIps();
                    txtStatus.Text = _devices.Count > 0
                        ? string.Format(LangResouorce.GetText("DevSearch_status_done"), _devices.Count)
                        : LangResouorce.GetText("DevSearch_status_done_none");
                });

            }
        }

        /// <summary>
        /// 解析设备响应报文
        /// </summary>
        private DevInfo ParseResponse(byte[] resp)
        {
            try
            {
                DevInfo dev = new DevInfo();
                int pos = 6;

                dev.StateCode = (resp[pos] << 24) | (resp[pos + 1] << 16) | (resp[pos + 2] << 8) | resp[pos + 3];
                pos += 4;

                dev.MacBytes = new byte[6];
                Array.Copy(resp, pos, dev.MacBytes, 0, 6);
                dev.Mac = BitConverter.ToString(resp, pos, 6).Replace('-', ':');
                pos += 6;

                dev.IsDhcp = resp[pos++] == 0x01;

                dev.Ip = string.Join(".", resp[pos], resp[pos + 1], resp[pos + 2], resp[pos + 3]);
                pos += 4;
                dev.NetMask = string.Join(".", resp[pos], resp[pos + 1], resp[pos + 2], resp[pos + 3]);
                pos += 4;
                dev.Gateway = string.Join(".", resp[pos], resp[pos + 1], resp[pos + 2], resp[pos + 3]);
                pos += 4;
                dev.Dns = string.Join(".", resp[pos], resp[pos + 1], resp[pos + 2], resp[pos + 3]);
                pos += 4;

                dev.LisPort = (resp[pos] << 8) | resp[pos + 1];
                pos += 2;

                dev.BoardType = resp[pos] == 0x01 ? "Hc32f46x" : LangResouorce.GetText("DevSearch_unknown");
                pos++;

                // 模块类型
                byte modType = resp[pos];
                if (modType == 0x31 || modType == 0x32 || modType == 0x33)
                {
                    // E710/E510/E310 系列，子类型在下一个字节
                    byte subType = resp[pos + 1];
                    string prefix;
                    switch (modType)
                    {
                        case 0x31: prefix = "SIM7"; break;
                        case 0x32: prefix = "SIM5"; break;
                        case 0x33: prefix = "SIM3"; break;
                        default: prefix = "SIM"; break;
                    }
                    switch (subType)
                    {
                        case 0: dev.ModType = prefix + "100"; break;
                        case 2: dev.ModType = prefix + "200"; break;
                        case 3: dev.ModType = prefix + "300"; break;
                        case 4: dev.ModType = prefix + "400"; break;
                        case 16: dev.ModType = prefix + "500"; break;
                        case 32: dev.ModType = prefix + "600"; break;
                        default: dev.ModType = LangResouorce.GetText("DevSearch_unknown"); break;
                    }
                }
                else if (modType == 0x34)
                {
                    dev.ModType = "SIM7100";
                }
                else
                {
                    dev.ModType = ModTypeMap.TryGetValue(modType, out string m) ? m : LangResouorce.GetText("DevSearch_unknown");
                }
                pos += 2;

                // 主板版本
                if (resp[pos] == 0 && resp[pos + 1] == 0 && resp[pos + 2] == 0 && resp[pos + 3] == 0)
                    dev.Bver = LangResouorce.GetText("DevSearch_unknown");
                else
                    dev.Bver = string.Join(".", resp[pos], resp[pos + 1], resp[pos + 2], resp[pos + 3]);
                pos += 4;

                // 模块版本
                if (resp[pos] == 0 && resp[pos + 1] == 0 && resp[pos + 2] == 0 && resp[pos + 3] == 0)
                    dev.Mver = LangResouorce.GetText("DevSearch_unknown");
                else
                    dev.Mver = resp[pos].ToString("X2") + "." + resp[pos + 1].ToString("X2") + "." +
                               resp[pos + 2].ToString("X2") + "." + resp[pos + 3].ToString("X2");
                pos += 4;

                // 工作模式
                switch (resp[pos])
                {
                    case 0x00: dev.Wmode = LangResouorce.GetText("DevSearch_bootloader"); break;
                    case 0x01: dev.Wmode = LangResouorce.GetText("DevSearch_passive"); break;
                    default: dev.Wmode = LangResouorce.GetText("DevSearch_active"); break;
                }

                return dev;
            }
            catch (Exception ex)
            {
                Debug.WriteLine("ParseResponse error: " + ex.Message);
                return null;
            }
        }


        private void btnClear_Click(object sender, RoutedEventArgs e)
        {
            _devices.Clear();
            _devDict.Clear();
            txtStatus.Text = "";
        }

        private void btnStop_Click(object sender, RoutedEventArgs e)
        {
            _cts?.Cancel();
        }

        private bool _isDialogOpen;
        private DateTime _lastOpenTime = DateTime.MinValue;

        /// <summary>
        /// 双击设备行打开网络配置窗口
        /// 注意：WPF DataGrid 的 MouseDoubleClick 在个别情况下会对同一次双击触发两次，
        /// 且 ShowDialog 是模态阻塞的——若第二次调用排队到第一个窗口关闭后才执行，
        /// 单纯防重入标志会被 finally 复位而失效。因此用 300ms 时间戳去抖 + 标志双保险。
        /// </summary>
        private void dgDevices_MouseDoubleClick(object sender, MouseButtonEventArgs e)
        {
            // 阻止事件继续路由（防止 DataGrid 内层元素重复冒泡触发）
            e.Handled = true;

            // 去抖：同一次双击引发的重复事件在 300ms 内直接忽略
            if ((DateTime.Now - _lastOpenTime).TotalMilliseconds < 300)
                return;
            _lastOpenTime = DateTime.Now;

            if (_isDialogOpen)
                return;

            if (dgDevices.SelectedItem is DevInfo dev)
            {
                _isDialogOpen = true;
                try
                {
                    Debug.WriteLine("[DevDetails] instance #" + (++_dbgInst));
                    var window = new DevDetailsWindow(dev, _selectedLocalIp);
                    window.Owner = Window.GetWindow(this);
                    window.ShowDialog();
                }
                finally
                {
                    _isDialogOpen = false;
                }
            }
        }

        private static int _dbgInst;
    }

    /// <summary>
    /// 支持 CancellationToken 的 Task 扩展
    /// </summary>
    internal static class TaskExtensions
    {
        public static async Task<UdpReceiveResult> WithCancellation(this Task<UdpReceiveResult> task, CancellationToken token)
        {
            var tcs = new TaskCompletionSource<UdpReceiveResult>();
            using (token.Register(() => tcs.TrySetCanceled()))
            {
                var completed = await Task.WhenAny(task, tcs.Task);
                return await completed;
            }
        }
    }
}
