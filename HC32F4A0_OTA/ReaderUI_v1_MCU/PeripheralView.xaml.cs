using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using ModuleTech;
using ReaderManager.Models;
using System.Diagnostics;

namespace ReaderManager
{
    /// <summary>
    /// PeripheralView.xaml 的交互逻辑
    /// </summary>
    public partial class PeripheralView : UserControl
    {
        public PeripheralView()
        {
            InitializeComponent();
            rpViewModel = ReaderParamsViewModel.GetReaderParamsViewModel();
            rootWnd = ReaderParamsViewModel.GetMainWindow();
            DataContext = rpViewModel;
            CtrlListSources.AddComboBox(cbbPsamSlot);
            btngpiList.Add(btnGpi1);
            btngpiList.Add(btnGpi2);
            btngpiList.Add(btnGpi3);
            btngpiList.Add(btnGpi4);
            
            btngpoList.Add(btnGpo1);
            btngpoList.Add(btnGpo2);
            btngpoList.Add(btnGpo3);
            btngpoList.Add(btnGpo4);
            
        }
        ReaderParamsViewModel rpViewModel = null;
        List<Button> btngpiList = new List<Button>();
        List<ToggleButton> btngpoList = new List<ToggleButton>();

        private void UserControl_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            tbRecvCmd.Width = tbRecvCmd.MinWidth * (rootWnd.ActualWidth / rootWnd.mainWndSize);
            tbSendCmd.Width = tbSendCmd.MinWidth * (rootWnd.ActualWidth / rootWnd.mainWndSize);
            foreach (RowDefinition rf in gdMain.RowDefinitions)
            {
                rf.Height = new GridLength(rf.MinHeight * (rootWnd.ActualHeight / rootWnd.mainWndHeightSize));
            }
        }

        MainWindow rootWnd = null;
        private void UserControl_Loaded(object sender, RoutedEventArgs e)
        {
            tbRecvCmd.Width = tbRecvCmd.MinWidth * (rootWnd.ActualWidth / rootWnd.mainWndSize);
            tbSendCmd.Width = tbSendCmd.MinWidth * (rootWnd.ActualWidth / rootWnd.mainWndSize);
            foreach (RowDefinition rf in gdMain.RowDefinitions)
            {
                rf.Height = new GridLength(rf.MinHeight * (rootWnd.ActualHeight / rootWnd.mainWndHeightSize));
            }
        }

        void GetGPIs_(bool istip)
        {
            try
            {
                GPIState[] gpis = rpViewModel.ModReader.GPIGet();
                for (int i = 0; i < gpis.Length; ++i)
                {
                    if (gpis[i].State)
                        btngpiList[i].SetResourceReference(Button.ContentProperty, "PeripheralView_GPIOstate_High");
                    else
                        btngpiList[i].SetResourceReference(Button.ContentProperty, "PeripheralView_GPIOstate_Low");
                }
            }
            catch (Exception ex)
            {
                ReaderParamsViewModel.GetMainWindow().TipApiFailed(LangResouorce.GetText("Msg_Params_GetFailed"), ex);
            }
            if (istip)
                ReaderParamsViewModel.GetMainWindow().TipSuccess(LangResouorce.GetText("Msg_Params_GetSuccess"));
        }
        private void btnGet_Click(object sender, RoutedEventArgs e)
        {
            GetGPIs_(false);
        }

        private void UserControl_IsVisibleChanged(object sender, DependencyPropertyChangedEventArgs e)
        {
            if ((bool)e.NewValue == true)
            {
                if (rpViewModel.IsConnect)
                    GetGPIs_(false);
            }
        }

        private void btnSet_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                for (int i = 0; i < btngpoList.Count; ++i)
                {
                    if (btngpoList[i].IsEnabled)
                    {
                        if ((bool)btngpoList[i].IsChecked)
                            rpViewModel.ModReader.GPOSet(i + 1, true);
                        else
                            rpViewModel.ModReader.GPOSet(i + 1, false);
                    }
                }
                rootWnd.TipSuccess(LangResouorce.GetText("Msg_Params_SetSuccess"));
            }
            catch (Exception ex)
            {
                rootWnd.TipApiFailed(LangResouorce.GetText("Msg_Params_SetFailed"), ex);
            }         
        }
    }
}
