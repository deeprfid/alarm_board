using Microsoft.Win32;
using ModuleLibrary;
using ModuleTech;
using ReaderManager.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Diagnostics;
using static ModuleTech.Reader;
using System.IO;
using System.Text.RegularExpressions;

namespace ReaderManager
{
    /// <summary>
    /// ToolsFirmwareUpdateView.xaml 的交互逻辑
    /// </summary>
    public partial class ToolsFirmwareUpdateView : UserControl
    {

        string latestVersion = "";//最新版本
        string currentVersion = "";//当前版本
        ReaderParamsViewModel rpViewModel = null;
        MainWindow rootWnd;

        public ToolsFirmwareUpdateView()
        {
            InitializeComponent();
            rpViewModel = ReaderParamsViewModel.GetReaderParamsViewModel();
            rootWnd = ReaderParamsViewModel.GetMainWindow();
            

            DataContext = rpViewModel;
        }

        private void UserControl_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            foreach (RowDefinition rf in gdMain.RowDefinitions)
            {
                rf.Height = new GridLength(rf.MinHeight * (rootWnd.ActualHeight / rootWnd.mainWndHeightSize));
            }
        }

        private void UserControl_Loaded(object sender, RoutedEventArgs e)
        {           
            foreach (RowDefinition rf in gdMain.RowDefinitions)
            {
                rf.Height = new GridLength(rf.MinHeight * (rootWnd.ActualHeight / rootWnd.mainWndHeightSize));
            }
        }

        private void btnBrowse_Click(object sender, RoutedEventArgs e)
        {
            OpenFileDialog of = new OpenFileDialog();
            if (IsIPAddress(rpViewModel.UpdFwReaderAddr))
                of.Filter = "固件|*.bin;*.slfw";
            else
                of.Filter = "固件|*.bin";

            Nullable<bool> result = of.ShowDialog();
            if (result == true)
                rpViewModel.UpdFwFilePath = of.FileName;
        }

        private void btnGet_Click(object sender, RoutedEventArgs e)
        {
            if (rpViewModel.UpdFwReaderAddr.Trim() == string.Empty)
            {
                rootWnd.TipMessage(LangResouorce.GetText("FirmwareUpdateView_Parmas_RdAddr") +
                   LangResouorce.GetText("Msg_Validation_is") +
                   LangResouorce.GetText("Msg_Validation_InvalidEmpty"));
                return;
            }
            try
            {
                rpViewModel.UpdFwVersion =(string)rpViewModel.ModReader.ParamGet("SoftwareVersion");
                currentVersion = rpViewModel.UpdFwVersion;
            }
            catch (Exception getverex)
            {
                ReaderParamsViewModel.GetMainWindow().TipApiFailed(LangResouorce.GetText("Msg_Params_GetFailed"), getverex);
            }
        }

        delegate void updateprog(float prg);
        void updateprogbar(float prg)
        {
            Dispatcher.BeginInvoke(new updateprog(updateui), prg);
        }

        //是否是ip连接
        public static bool IsIPAddress(string str)
        {
            if (str == null || str == string.Empty || str.Length < 7 || str.Length > 15) return false;

            string regformat = @"^\d{1,3}[\.]\d{1,3}[\.]\d{1,3}[\.]\d{1,3}$";

            Regex regex = new Regex(regformat, RegexOptions.IgnoreCase);
            return regex.IsMatch(str);
        }
        void updateui(float prog)
        {
            int curstep = (int)(prog * 100);
            rpViewModel.UpFwProgress = curstep;
            rpViewModel.UpdFwPercent = curstep.ToString() + "%";
        }

        delegate void UpdateUiTip(string tip, Exception ex);
        void AfterUpdate(string tip, Exception ex)
        {
            if (ex == null)
            {
                rpViewModel.UpFwProgress = 100;
                rpViewModel.UpdFwPercent = "100%";
                rootWnd.TipSuccess(tip);
            }
            else
                rootWnd.TipApiFailed(tip, ex);

            rpViewModel.IsUpdFw = false;
        }

        void updatefirmware()
        {
            try
            {
                Reader.FirmwareLoadEx(rpViewModel.UpdFwReaderAddr.Trim(), rpViewModel.UpdFwFilePath, 
                    new Reader.FirmwareUpdate(updateprogbar));

                Dispatcher.BeginInvoke(new UpdateUiTip(AfterUpdate),
                    new object[] { LangResouorce.GetText("Msg_FirmwareUpdateView_UpdateOK"), null });
            }
            catch (OpFaidedException opfex)
            {
                try
                {
                    string exepath = AppDomain.CurrentDomain.SetupInformation.ApplicationBase;
                    if (opfex.ErrCode == 0x9025)
                    {
                        Reader.BootloaderLoad(rpViewModel.UpdFwReaderAddr.Trim(), exepath + "\\btfiles\\m5e_bt_1.0.0.0_old.maf", new Reader.FirmwareUpdate(updateprogbar));
                    }
                    else if (opfex.ErrCode == 0x9026)
                    {
                        Reader.BootloaderLoad(rpViewModel.UpdFwReaderAddr.Trim(), exepath + "\\btfiles\\m5e_bt_1.0.0.0_new.maf", new Reader.FirmwareUpdate(updateprogbar));
                    }
                    else
                    {
                        Dispatcher.BeginInvoke(new UpdateUiTip(AfterUpdate),
                            new object[] { LangResouorce.GetText("Msg_FirmwareUpdateView_UpdateFailed"), opfex });
                        return;
                    }
                    //                    MessageBox.Show("bootloader修改完成,等待读写器指示灯变成闪烁状态后使读写器重新上电，当读写器重新上电后再点击‘确定’按钮，将执行固件升级第二阶段");
                    Reader.FirmwareLoadEx(rpViewModel.UpdFwReaderAddr.Trim(),
                        rpViewModel.UpdFwFilePath, new Reader.FirmwareUpdate(updateprogbar));
                    Dispatcher.BeginInvoke(new UpdateUiTip(AfterUpdate),
                        new object[] { LangResouorce.GetText("Msg_FirmwareUpdateView_UpdateOK"), null });
                }
                catch (Exception btex)
                {
                    Dispatcher.BeginInvoke(new UpdateUiTip(AfterUpdate),
                        new object[] { LangResouorce.GetText("Msg_FirmwareUpdateView_UpdateFailed"), btex });
                }
            }
            catch (Exception eex)
            {
                Dispatcher.BeginInvoke(new UpdateUiTip(AfterUpdate),
                    new object[] { LangResouorce.GetText("Msg_FirmwareUpdateView_UpdateFailed"), eex });
            }
        }

        private void btnUpdate_Click(object sender, RoutedEventArgs e)
        {
            if (rpViewModel.UpdFwReaderAddr.Trim() == string.Empty)
            {
                rootWnd.TipMessage(LangResouorce.GetText("FirmwareUpdateView_Parmas_RdAddr") +
                   LangResouorce.GetText("Msg_Validation_is") +
                   LangResouorce.GetText("Msg_Validation_InvalidEmpty"));
                return;
            }
            if (rpViewModel.UpdFwFilePath.Trim() == string.Empty)
            {
                rootWnd.TipMessage(LangResouorce.GetText("Msg_FirmwareUpdateView_SelectFile"));
                return;
            }

            rpViewModel.IsUpdFw = true;
            rpViewModel.UpFwProgress = 0;
            rpViewModel.UpdFwPercent = "0%";
            new Thread(new ThreadStart(delegate ()
            {
                updatefirmware();
            })).Start();
        }

        private void SoftwareVersionClick(object sender, RoutedEventArgs e) {

            string file = "SoftwareVersion";
            string com = rpViewModel.UpdFwReaderAddr.Trim();
            DirectoryInfo directoryInfo = new DirectoryInfo(AppDomain.CurrentDomain.BaseDirectory);
            string downloadSite = directoryInfo.Parent.FullName + "/download";

            //将文件下载到download
            var downloadBool = FtpHelper.DownLoadDirectory(file + "/" + latestVersion, downloadSite);
            
            //下载文件成功就自动升级
            if (downloadBool != null&& downloadBool.Count>0) {

                //断开连接
                if (rpViewModel.IsConnect)
                {
                    rpViewModel.ModReader.Disconnect();
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

                //重新赋值 升级需要的参数
                rpViewModel.UpdFwFilePath = downloadSite+"/"+ downloadBool[0].Trim();
                rpViewModel.UpdFwVersion = currentVersion;
                rpViewModel.UpdFwReaderAddr = com;
                rpViewModel.IsUpdFw = true;
                rpViewModel.UpFwProgress = 0;
                rpViewModel.UpdFwPercent = "0%";
                new Thread(new ThreadStart(delegate ()
                {
                    updatefirmware();
                })).Start();
            }
        }
       
        private void GdMain_Loaded(object sender, RoutedEventArgs e)
        {
            //每次打开页面都先隐藏
            SoftwareVersion.Visibility = Visibility.Collapsed;

            if (rpViewModel == null|| rpViewModel.IsConnect==false) {
                return;
            }

            try
            {
                rpViewModel.UpdFwVersion = (string)rpViewModel.ModReader.ParamGet("SoftwareVersion");
                currentVersion = rpViewModel.UpdFwVersion;
            }
            catch (Exception getverex)
            {
                ReaderParamsViewModel.GetMainWindow().TipApiFailed(LangResouorce.GetText("Msg_Params_GetFailed"), getverex);
            }

            //获取最新版本，判断是否显示更新按钮
            var list = FtpHelper.GetFtpFileInfos("", "SoftwareVersion");
            list = list.OrderByDescending(r => r.FileName).ToArray();
            if (list != null&& list.Count()>0 && list[0].FileName != "." && list[0].FileName != "..")
            {
                Version v1 = new Version(list[0].FileName);
                Version v2 = new Version(rpViewModel.UpdFwVersion);
                if (v1 > v2)
                {
                    SoftwareVersion.Visibility = Visibility.Visible;
                    latestVersion = list[0].FileName;
                }
            }

        }
    }
}
