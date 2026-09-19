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
using Microsoft.Win32;
using ModuleLibrary;
using ModuleTech;
using ReaderManager.Models;

namespace ReaderManager
{
    /// <summary>
    /// FirmwareUpdateView.xaml 的交互逻辑
    /// </summary>
    public partial class FirmwareUpdateView : UserControl
    {
        public FirmwareUpdateView()
        {
            InitializeComponent();
            rpViewModel = ReaderParamsViewModel.GetReaderParamsViewModel();
            rootWnd = ReaderParamsViewModel.GetMainWindow();
            DataContext = rpViewModel;
        }
        ReaderParamsViewModel rpViewModel = null;
        private void UserControl_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            //tbTip1.Width = tbTip1.MinWidth * (rootWnd.ActualWidth / rootWnd.mainWndSize);
            //tbTip2.Width = tbTip2.MinWidth * (rootWnd.ActualWidth / rootWnd.mainWndSize);
            //pbFwUpdProg.Width = pbFwUpdProg.MinWidth * (rootWnd.ActualWidth / rootWnd.mainWndSize);
            double width = 0;
            if ((ActualWidth - 60) / 26 * 3 <= 60)
                width = ActualWidth - 120 - 20;
            else
                width = (ActualWidth - 60) / 26 * 23 - 20;
            tbTip1.Width = width;
            tbTip2.Width = width;
            pbFwUpdProg.Width = width;

            foreach (RowDefinition rf in gdMain.RowDefinitions)
            {
                rf.Height = new GridLength(rf.MinHeight * (rootWnd.ActualHeight / rootWnd.mainWndHeightSize));
            }
        }

        MainWindow rootWnd;
        private void UserControl_Loaded(object sender, RoutedEventArgs e)
        {
            //tbTip1.Width = tbTip1.MinWidth * (rootWnd.ActualWidth / rootWnd.mainWndSize);
            //tbTip2.Width = tbTip2.MinWidth * (rootWnd.ActualWidth / rootWnd.mainWndSize);
            //pbFwUpdProg.Width = pbFwUpdProg.MinWidth * (rootWnd.ActualWidth / rootWnd.mainWndSize);
            double width = 0;
            if ((ActualWidth - 60) / 26 * 3 <= 60)
                width = ActualWidth - 120 - 20;
            else
                width = (ActualWidth - 60) / 26 * 23 - 20;
            tbTip1.Width = width;
            tbTip2.Width = width;
            pbFwUpdProg.Width = width;

            foreach (RowDefinition rf in gdMain.RowDefinitions)
            {
                rf.Height = new GridLength(rf.MinHeight * (rootWnd.ActualHeight / rootWnd.mainWndHeightSize));
            }
        }

        private void btnBrowse_Click(object sender, RoutedEventArgs e)
        {
            OpenFileDialog of = new OpenFileDialog();
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
                rpViewModel.UpdFwVersion = Reader.GetSoftVersion(rpViewModel.UpdFwReaderAddr.Trim());
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

        void updateui(float prog)
        {
            int curstep = (int)(prog * 100);
            rpViewModel.UpFwProgress = curstep;
            rpViewModel.UpdFwPercent = curstep.ToString()+"%";
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
                Reader.FirmwareLoadEx(rpViewModel.UpdFwReaderAddr.Trim(), 
                    rpViewModel.UpdFwFilePath, new Reader.FirmwareUpdate(updateprogbar));
                Dispatcher.BeginInvoke(new UpdateUiTip(AfterUpdate), 
                    new object[] { LangResouorce.GetText("Msg_FirmwareUpdateView_UpdateOK"), null });
            }
            catch (OpFaidedException opfex)
            {
                try
                {
                    string exepath = Application.Current.StartupUri.AbsolutePath;
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
    }
}
