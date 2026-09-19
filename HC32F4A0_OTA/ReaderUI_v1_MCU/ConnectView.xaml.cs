// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

using Microsoft.Win32;
using ModuleTech;
using ReaderManager.Models;
using System;
using System.Collections.Generic;
using System.Configuration;
using System.Diagnostics;
using System.Linq;
using System.Threading;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

namespace ReaderManager
{
    /// <summary>
    /// Interaction logic for AboutView.xaml
    /// </summary>
    public partial class ConnectView : UserControl
    {
        public ConnectView()
        {
            InitializeComponent();
            rootWnd = ReaderParamsViewModel.GetMainWindow();
            //CtrlListSources.SetLang();
            //cbbDevType.ItemsSource = CtrlListSources.connect_DevAntTypesList;
            rpViewModel = ReaderParamsViewModel.GetReaderParamsViewModel();
            tbBoardType.DataContext = rpViewModel;
            tbModType.DataContext = rpViewModel;
            tbHardwareVer.DataContext = rpViewModel;
            tbSoftwareVer.DataContext = rpViewModel;
            btnDisconnect.DataContext = rpViewModel;
            tbxReaderAddr.DataContext = rpViewModel;
            btnConnect.DataContext = rpViewModel;
            tbSoftwareVer1.DataContext = rpViewModel;
            CtrlListSources.AddComboBox(cbbDevType);

            //显示版本信息
            var versions = ConfigurationManager.AppSettings["versions"];
            if (Thread.CurrentThread.CurrentCulture.Name != "zh-CN")
                return;

            FtpHelper.path = "ftp://" + ConfigurationManager.AppSettings["ftpUrl"];
            rpViewModel.Versions = versions;
            SoftwareVersions.Text = versions;
            latestVersion =versions;

            if (FtpHelper.IsInternetAvailable())
            {
                //获取最新版本，判断是否显示更新按钮
                var list = FtpHelper.GetFtpFileInfos("", "ReaderUI");
                list = list.OrderByDescending(r => r.FileName).ToArray();
                if (list != null&& list.Length>0 && list[0].FileName != "." && list[0].FileName != "..")
                {
                    if (Convert.ToDouble(list[0].FileName) > Convert.ToDouble(rpViewModel.Versions))
                    {
                        VersionUpdates.Visibility = Visibility.Visible;
                        latestVersion = list[0].FileName;
                    }
                }
            }
        }
        MainWindow rootWnd = null;
        ReaderParamsViewModel rpViewModel = null;
        string latestVersion ="";//最新版本

        private void btnConnect_Click(object sender, System.Windows.RoutedEventArgs e)
        {
            if (cbbDevType.SelectedIndex == -1)
            {
                ReaderParamsViewModel.GetMainWindow().TipMessage(LangResouorce.GetText("Msg_Connect_SelectDevType"));
                return;
            }
            else
            {
                if (cbbDevType.SelectedIndex == 0)
                    rpViewModel.AntPortNumber = 1;
                else if (cbbDevType.SelectedIndex == 1)
                    rpViewModel.AntPortNumber = 2;
                else if (cbbDevType.SelectedIndex == 2)
                    rpViewModel.AntPortNumber = 4;
                else if (cbbDevType.SelectedIndex == 3)
                    rpViewModel.AntPortNumber = 8;
                else if (cbbDevType.SelectedIndex == 4)
                    rpViewModel.AntPortNumber = 16;
            }
            if (rpViewModel.ReaderAddr == string.Empty|| rpViewModel.ReaderAddr == null)
            {
                ReaderParamsViewModel.GetMainWindow().TipMessage(LangResouorce.GetText("Msg_Connect_InputRdrAddr"));
                return;
            }
            try
            {
                try
                {
                    App.ConnIp = rpViewModel.ReaderAddr;
                    rpViewModel.ModReader = Reader.Create(rpViewModel.ReaderAddr, Region.NA, rpViewModel.AntPortNumber);
                }
                catch (Exception cnnex)
                {
                    ReaderParamsViewModel.GetMainWindow().TipApiFailed(LangResouorce.GetText("Msg_Connect_ConnectFailed"), cnnex);
                    return;
                }

                try
                {
                    rpViewModel.ModReader.ParamGet("HopAntTime");
                }
                catch
                {
                    rpViewModel.EnableAntMaxDwellTime = false;
                }

                Is5300or3500(rpViewModel.ModReader.HwDetails.module);
                if (rpViewModel.ModReader != null && (
                    rpViewModel.ModReader.HwDetails.module == Reader.Module_Type.MODOULE_SLR5100 ||
                    rpViewModel.ModReader.HwDetails.module == Reader.Module_Type.MODOULE_SLR5200 ||
                    rpViewModel.ModReader.HwDetails.module == Reader.Module_Type.MODOULE_SLR5300))
                {
                    rpViewModel.IsFastInvMode = false;
                }

                rpViewModel.UpdFwReaderAddr = rpViewModel.ReaderAddr;
                rpViewModel.BoardType = rpViewModel.ModReader.HwDetails.board.ToString();
                rpViewModel.ModType = rpViewModel.ModReader.HwDetails.module.ToString();


                rpViewModel.HardwareVer = (string)rpViewModel.ModReader.ParamGet("HardwareVersion");
                rpViewModel.SoftwareVer = (string)rpViewModel.ModReader.ParamGet("SoftwareVersion");
                //rpViewModel.MainboardVer = (string)rpViewModel.ModReader.ParamGet("MainboardSoftwareVersion");

                rpViewModel.MaxTxPower = (int)rpViewModel.ModReader.ParamGet("RfPowerMax");
                rpViewModel.MinTxPower = (int)rpViewModel.ModReader.ParamGet("RfPowerMin");

                rpViewModel.UniByAnt = (bool)rpViewModel.ModReader.ParamGet("IsTagDataUniqueByAnt");
                rpViewModel.UniByBankData = (bool)rpViewModel.ModReader.ParamGet("IsTagDataUniqueByEmddata");

                rpViewModel.IsConnect = true;

                if (rpViewModel.AntPortNumber == 1)
                    rpViewModel.InvAnts = new int[1] { 1 };
                else
                    rpViewModel.InvAnts = (int[])rpViewModel.ModReader.ParamGet("ConnectedAntennas");

                rpViewModel.ModReader.ReadException += ReadingErrHandler;
                rpViewModel.ModReader.TagsRead += ((InventoryView)ReaderParamsViewModel.GetMainWindow().AllViews["InventoryView"]).OnTagRead;

                //GPIState[] gpis = rpViewModel.ModReader.GPIGet();
                //if (gpis.Length == 2)
                //{
                //    rpViewModel.EnableGpi1 = true;
                //    rpViewModel.EnableGpi2 = true;
                //}
                //else if (gpis.Length == 4)
                //{
                //    rpViewModel.EnableGpi1 = true;
                //    rpViewModel.EnableGpi2 = true;
                //    rpViewModel.EnableGpi3 = true;
                //    rpViewModel.EnableGpi4 = true;
                //}

                if (rpViewModel.ModReader.HwDetails.board == Reader.MaindBoard_Type.MAINBOARD_ARM9 ||
                    rpViewModel.ModReader.HwDetails.board == Reader.MaindBoard_Type.MAINBOARD_ARM9_WIFI)
                    rpViewModel.EnableFreqHopMode = false;

                 rpViewModel.EnableWireless = rpViewModel.ModReader.HwDetails.board == Reader.MaindBoard_Type.MAINBOARD_ARM9_WIFI;
                

                if (rpViewModel.ModReader.HwDetails.board == Reader.MaindBoard_Type.MAINBOARD_SERIAL)
                {
                    rpViewModel.EnableIp = false;
                    rpViewModel.EnableGpo1 = true;
                    rpViewModel.EnableGpo2 = true;
                }
                else
                {
                    rpViewModel.EnableIp = true;
                    rpViewModel.EnableGpo1 = true;
                    rpViewModel.EnableGpo2 = true;
                    rpViewModel.EnableGpo3 = true;
                    rpViewModel.EnableGpo4 = true;
                }
                
                if(!rpViewModel.Is5300or3500)
                    ((ParamAntsView)ReaderParamsViewModel.GetMainWindow().AllViews["ParamAntsView"]).GetParams();
                


            }
            catch (Exception ex)
            {
                rpViewModel.ModReader.Disconnect();
                ReaderParamsViewModel.GetMainWindow().TipApiFailed(LangResouorce.GetText("Msg_Connect_GetParamsFailed"), ex);
                return;
            }
        }

        private void btnDisconnect_Click(object sender, System.Windows.RoutedEventArgs e)
        {
            if (rpViewModel.IsConnect)
            {
                rpViewModel.ModReader.Disconnect();
                rpViewModel.ModReader=null;
                rpViewModel.ResetReaderParams();
                ((ParamAntsView)rootWnd.AllViews["ParamAntsView"]).ResetView();
                ((ParamInvView)rootWnd.AllViews["ParamInvView"]).ResetView();
                ((ParamHwInterfaceView)rootWnd.AllViews["ParamHwInterfaceView"]).ResetView();
                ((ParamAdvanceView)rootWnd.AllViews["ParamAdvanceView"]).ResetView();
                ((ParamEasView)rootWnd.AllViews["ParamEasView"]).ResetView();
                ((ParamWhiteView)rootWnd.AllViews["ParamWhiteView"]).ResetView();
                ((ParamTagView)rootWnd.AllViews["ParamTagView"]).ResetView();
                ((ParamLogView)rootWnd.AllViews["ParamLogView"]).ResetView();
                ((ParamStoreView)rootWnd.AllViews["ParamStoreView"]).ResetView();

            }
        }

        private void tbxReaderAddr_GotFocus(object sender, System.Windows.RoutedEventArgs e)
        {
            if (tbxReaderAddr.Text == String.Empty)
            {
                tbxReaderAddr.Text = "192.168.1.100";
            }
        }

        class ReaderExceptionChecker
        {
            public ReaderExceptionChecker(int maxerrcnt, int dursec)
            {
                dts = new DateTime[maxerrcnt];
                index = 0;
                maxdursec = dursec;
            }
            public void AddErr()
            {
                dts[index++] = DateTime.Now;
            }

            DateTime[] dts;
            readonly int maxdursec;
            int index;
            public bool IsTrigger()
            {
                DateTime now = DateTime.Now;
                if (index == dts.Length - 1)
                {
                    if (now.Subtract(dts[0]).TotalSeconds < maxdursec)
                    {
                        return true;
                    }
                    else
                    {
                        index = 0;
                        return false;
                    }
                }
                else
                {
                    return false;
                }
            }
        }

        delegate void ReconnectHandler(int reason, Exception ex);
        void Reconnect(int reason, Exception ex)
        {
            if (reason == 0)
            {
                rpViewModel.ModReader.TagsRead += ((InventoryView)ReaderParamsViewModel.GetMainWindow().AllViews["InventoryView"]).OnTagRead;
                rpViewModel.ModReader.ReadException += ReadingErrHandler;
                rpViewModel.IsConnect = true;
                ((InventoryView)ReaderParamsViewModel.GetMainWindow().AllViews["InventoryView"]).btnStart_Click(null, null);
            }
            else
            {
                Debug.WriteLine("Reconnect.InventoryErrLo:" + rpViewModel.InventoryErrLog);
                if (rpViewModel.ModReader != null)
                {
                    rpViewModel.ModReader.Disconnect();
                }

                if (reason == 1)
                {
                    ReaderParamsViewModel.GetMainWindow().TipMessage(
                        LangResouorce.GetText("Msg_Connect_ReconnectFailed") +
                        LangResouorce.GetText("Msg_Comma") +
                        LangResouorce.GetText("Msg_ErrInfo") +
                        LangResouorce.GetText("Msg_Colon") +
                        rpViewModel.InventoryErrLog +
                         LangResouorce.GetText("Msg_Comma") +
                         ex.ToString());
                }
                else if (reason == 2)
                {
                    ReaderParamsViewModel.GetMainWindow().TipMessage(
                        LangResouorce.GetText("Msg_Inventory_ErrorFrquently") +
                        LangResouorce.GetText("Msg_Comma") +
                        LangResouorce.GetText("Msg_ErrInfo") +
                        LangResouorce.GetText("Msg_Colon") +
                        rpViewModel.InventoryErrLog +
                         LangResouorce.GetText("Msg_Comma") +
                         ex.ToString());
                }

                rpViewModel.ResetReaderParams();
            }
        }

        ReaderExceptionChecker rechecker = new ReaderExceptionChecker(4, 60);
        void ReadingErrHandler(object sender, Reader.ReadExceptionEventArgs expArgs)
        {
            Exception ex = expArgs.ReaderException;
            Debug.WriteLine("读写器" + ((Reader)sender).Address + "错误:" + ex.ToString());
            rpViewModel.InventoryErrLog += ex.ToString();
            rpViewModel.InventoryErrLog += "\n";
            Debug.WriteLine("InventoryErrLog:" + rpViewModel.InventoryErrLog);
            Reader expreader = (Reader)sender;
            if (rechecker.IsTrigger())
            {
                Dispatcher.BeginInvoke(new ReconnectHandler(Reconnect), new object[] { 2, expArgs.ReaderException });
                return;
            }
            else
            {
                rechecker.AddErr();
            }

            string rdraddress = expreader.Address;
            expreader.Disconnect();
            int reconnectcnt = 1;
            int connectinterval = 2;
            rpViewModel.ModReader = null;
            for (int i = 0; i < reconnectcnt; ++i)
            {
                try
                {
                    rpViewModel.ModReader = Reader.Create(rdraddress, ModuleTech.Region.NA, rpViewModel.AntPortNumber);
                    Dispatcher.BeginInvoke(new ReconnectHandler(Reconnect), new object[] { 0, null });
                    break;
                }
                catch (Exception conex)
                {
                    if (i == reconnectcnt - 1)
                    {
                        Dispatcher.BeginInvoke(new ReconnectHandler(Reconnect), new object[] { 1, conex });
                        return;
                    }
                    else
                    {
                        Thread.Sleep(connectinterval * 1000);
                    }
                }
            }
        }

        private void IpClick(object sender, System.Windows.RoutedEventArgs e)
        {
            tbxReaderAddr.Text = "192.168.1.100";
            Keyboard.Focus(tbxReaderAddr);
        }
        private void ComClick(object sender, System.Windows.RoutedEventArgs e)
        {
            var com = Find_WIN32_Com();
            if (com.Length > 0)
            {
                tbxReaderAddr.Text = com[0];
            }
            else
            {
                tbxReaderAddr.Text = "COM";
            }

            Keyboard.Focus(tbxReaderAddr);
        }

        public string[] Find_WIN32_Com()
        {
            List<string> coms = new List<string>();
            RegistryKey keyCom = Registry.LocalMachine.OpenSubKey("Hardware\\DeviceMap\\SerialComm");
            if (keyCom != null)
            {
                string[] sSubKeys = keyCom.GetValueNames();

                for (int i = 0; i < sSubKeys.Length; i++)
                {
                    if (!sSubKeys[i].Contains("BthModem"))
                    {
                        coms.Add((string)keyCom.GetValue(sSubKeys[i]));
                    }
                }
            }
            return coms.ToArray();
        }

        private void TextBlock_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
        {
            var url = AppDomain.CurrentDomain.BaseDirectory+ "/versionUpdating/ReaderUpgrade.exe";
            var ftpVersions = ConfigurationManager.AppSettings["versions"];
            var ftpFile = ConfigurationManager.AppSettings["ftpFile"];
            var ftpUrl = ConfigurationManager.AppSettings["ftpUrl"];
            var exe = ConfigurationManager.AppSettings["exe"];

            string arguments = $"{ftpFile}+{latestVersion}+{exe}+{ftpUrl}";

            ProcessStartInfo info = new ProcessStartInfo(url, arguments);
            Process process = new Process();
            process.StartInfo = info;
            process.Start();

            Window window = Window.GetWindow(this);//关闭窗体
            window.Close();
        }

        private void cbbDevType_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (rpViewModel.ModReader != null && rpViewModel.Is5300or3500)
            {
                if (rpViewModel.EnableAntMaxDwellTime) { 
                    rpViewModel.AntMaxDwellTime = ((int)rpViewModel.ModReader.ParamGet("HopAntTime")).ToString();
                }
                rpViewModel.ModReader.ParamSet("GpoTarget", 1);

                if (cbbDevType.SelectedIndex == 2)
                {
                    rpViewModel.AntPortNumber = 4;
                    ((ParamAntsView)ReaderParamsViewModel.GetMainWindow().AllViews["ParamAntsView"]).GetParams();
                }
                else if (cbbDevType.SelectedIndex == 0)
                {
                    rpViewModel.AntPortNumber = 1;
                }
            }
        }

        private void Is5300or3500(Reader.Module_Type module_Type) {
            if (module_Type == Reader.Module_Type.MODOULE_SLR5300)
                rpViewModel.Is5300or3500 =true;
            else if (module_Type == Reader.Module_Type.MODOULE_SIM3500)
                rpViewModel.Is5300or3500=true;
            else
                rpViewModel.Is5300or3500=false;
        
            //if(rpViewModel.Is5300or3500)
            //    cbbDevType.SelectedIndex = 2;
        }

    }
}