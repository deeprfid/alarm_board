using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
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
using ReaderManager.Models;

namespace ReaderManager
{
    /// <summary>
    /// ParamSettings.xaml 的交互逻辑
    /// </summary>
    public partial class ParamSettingsView : UserControl
    {
        public ParamSettingsView()
        {
            InitializeComponent();
            DataContext = ReaderParamsViewModel.GetReaderParamsViewModel();
        }
        private void TabControl_Selected(object sender, RoutedEventArgs e)
        {
            string currentTime = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"); // 设置日期和时间的显示格式
            Console.WriteLine(currentTime);
        }
        public void SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            ((ParamTagView)ReaderParamsViewModel.GetMainWindow().AllViews["ParamTagView"]).Leave();
            if (tcParamMuem.SelectedIndex == 4)
            {
                ((ParamEasView)ReaderParamsViewModel.GetMainWindow().AllViews["ParamEasView"]).InitParams(true);
            }
            if (tcParamMuem.SelectedIndex == 5)
            {
                ((ParamWhiteView)ReaderParamsViewModel.GetMainWindow().AllViews["ParamWhiteView"]).InitParams(true);
            }
            if (tcParamMuem.SelectedIndex == 6)
            {
                ((ParamTagView)ReaderParamsViewModel.GetMainWindow().AllViews["ParamTagView"]).InitParams(true);
            }
            if (tcParamMuem.SelectedIndex == 7)
            {
                ((ParamLogView)ReaderParamsViewModel.GetMainWindow().AllViews["ParamLogView"]).InitParams(true);
            }
            if (tcParamMuem.SelectedIndex != 8)
            {
                ((ParamTagView)ReaderParamsViewModel.GetMainWindow().AllViews["ParamTagView"]).InitParams(true);
            }
            if (tcParamMuem.SelectedIndex != 9)
            {
                ((ParamStoreView)ReaderParamsViewModel.GetMainWindow().AllViews["ParamStoreView"]).InitParams(true);
            }
        }

        private void btnGet_Click(object sender, RoutedEventArgs e)
        {
            if (tcParamMuem.SelectedIndex == 0)
            {
                ((ParamAntsView)ReaderParamsViewModel.GetMainWindow().AllViews["ParamAntsView"]).GetParams(true);
            }
            else if (tcParamMuem.SelectedIndex == 1)
            {
                ((ParamInvView)ReaderParamsViewModel.GetMainWindow().AllViews["ParamInvView"]).GetParams(true);
            }
            else if (tcParamMuem.SelectedIndex == 2)
            {
                ((ParamHwInterfaceView)ReaderParamsViewModel.GetMainWindow().AllViews["ParamHwInterfaceView"]).GetParams(true);
            }
            else if (tcParamMuem.SelectedIndex == 3)
            {
                ((ParamAdvanceView)ReaderParamsViewModel.GetMainWindow().AllViews["ParamAdvanceView"]).GetParams(true);
            }

        }

        private void btnSet_Click(object sender, RoutedEventArgs e)
        {
            if (tcParamMuem.SelectedIndex == 0)
            {
                ((ParamAntsView)ReaderParamsViewModel.GetMainWindow().AllViews["ParamAntsView"]).SetParams();
            }
            else if (tcParamMuem.SelectedIndex == 1)
            {
                ((ParamInvView)ReaderParamsViewModel.GetMainWindow().AllViews["ParamInvView"]).SetParams();
            }
            else if (tcParamMuem.SelectedIndex == 2)
            {
                ((ParamHwInterfaceView)ReaderParamsViewModel.GetMainWindow().AllViews["ParamHwInterfaceView"]).SetParams();
            }
            else if (tcParamMuem.SelectedIndex == 3)
            {
                ((ParamAdvanceView)ReaderParamsViewModel.GetMainWindow().AllViews["ParamAdvanceView"]).SetParams();
            }

        }

        public void btnPerpetualSet_Click(object sender, RoutedEventArgs e) {

            if (tcParamMuem.SelectedIndex == 0)
            {
                ((ParamAntsView)ReaderParamsViewModel.GetMainWindow().AllViews["ParamAntsView"]).SetPerpetualParams();
            }
            else if (tcParamMuem.SelectedIndex == 1)
            {
                ((ParamInvView)ReaderParamsViewModel.GetMainWindow().AllViews["ParamInvView"]).SetPerpetualParams();
            }
            else if (tcParamMuem.SelectedIndex == 2)
            {
                ((ParamHwInterfaceView)ReaderParamsViewModel.GetMainWindow().AllViews["ParamHwInterfaceView"]).SetParams();
            }
            else if (tcParamMuem.SelectedIndex == 3)
            {
                ((ParamAdvanceView)ReaderParamsViewModel.GetMainWindow().AllViews["ParamAdvanceView"]).SetPerpetualParams();
            }

        }



    }
}
