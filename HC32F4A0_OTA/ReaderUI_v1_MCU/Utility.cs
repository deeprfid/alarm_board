using System.Windows;
using System.Windows.Controls;
using MahApps.Metro.Controls;
using MahApps.Metro.Controls.Dialogs;
using System.Diagnostics;
using System.Data;
using System.Linq;
using System.Windows.Input;
using System.Threading;
using System;
using System.Collections.Generic;
using System.Windows.Media;
using System.IO;

namespace ReaderManager
{
    internal class Utility
    {
        /// <summary>
        /// 全局设备 IP 列表，搜索到的设备 IP 会追加进来（设备搜索移植 v2）
        /// </summary>
        public static List<string> DeviceIps { get; private set; } = new List<string>();

        /// <summary>
        /// 从 curDev.csv 初始化 DeviceIps 列表
        /// </summary>
        public static void InitDeviceIps()
        {
            DeviceIps.Clear();
            string devPath = AppDomain.CurrentDomain.BaseDirectory + "curDev.csv";
            try
            {
                if (File.Exists(devPath))
                {
                    foreach (var line in File.ReadAllLines(devPath))
                    {
                        string ip = line.Trim();
                        if (!string.IsNullOrEmpty(ip) && !DeviceIps.Contains(ip))
                            DeviceIps.Add(ip);
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine("InitDeviceIps error: " + ex.Message);
            }
        }

        /// <summary>
        /// 从 DeviceIps 列表填充 ComboBox
        /// </summary>
        public static void LoadDeviceIps(ComboBox cb)
        {
            cb.Items.Clear();
            foreach (var ip in DeviceIps)
                cb.Items.Add(ip);
            if (DeviceIps.Count > 0)
                cb.Text = DeviceIps[0];
        }

        /// <summary>
        /// 替换全局设备 IP 列表并写入 curDev.csv
        /// </summary>
        public static void ReplaceDeviceIps(List<string> ips)
        {
            DeviceIps = ips ?? new List<string>();
            try
            {
                string devPath = AppDomain.CurrentDomain.BaseDirectory + "curDev.csv";
                File.WriteAllLines(devPath, DeviceIps);
            }
            catch (Exception ex)
            {
                Console.WriteLine("ReplaceDeviceIps error: " + ex.Message);
            }
        }

        /// <summary>
        /// 刷新主窗口所有视图的 cbDevIp 下拉框
        /// </summary>
        public static void RefreshAllDeviceIps()
        {
            var mainWnd = Application.Current.MainWindow as MainWindow;
            if (mainWnd == null) return;

            var viewNames = new string[]
            {
                "ParamEasView", "ParamWhiteView", "ParamDisplayView",
                "ParamTagView", "ParamLogView", "ParamStoreView", "ParamDevSet"
            };

            foreach (var name in viewNames)
            {
                if (mainWnd.AllViews.TryGetValue(name, out object view))
                {
                    var cbField = view.GetType().GetField("cbDevIp",
                        System.Reflection.BindingFlags.Instance |
                        System.Reflection.BindingFlags.Public |
                        System.Reflection.BindingFlags.NonPublic);
                    if (cbField?.GetValue(view) is System.Windows.Controls.ComboBox cb)
                    {
                        cb.Items.Clear();
                        foreach (var ip in DeviceIps)
                            cb.Items.Add(ip);
                        if (DeviceIps.Count > 0)
                            cb.SelectedIndex = 0;
                    }
                }
            }
        }

        public static void EnableButton(Button btn, Color clFore)
        {
            btn.IsEnabled = true;
            
        //    btn.BorderBrush = new SolidColorBrush(Color.FromArgb(0xff, 0x0E, 0x31, 0x93));
            btn.Foreground = new SolidColorBrush(clFore);
            
        }

        public static void DisableButton(Button btn)
        {
            btn.IsEnabled = false;
            
        //    btn.BorderBrush = new SolidColorBrush(Colors.DarkSlateGray);
            btn.Foreground = new SolidColorBrush(Colors.Gray);
            
        }
    }
}
