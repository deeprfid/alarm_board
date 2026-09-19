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
using ReaderManager.Models;
using ModuleTech;

namespace ReaderManager
{
    /// <summary>
    /// FlyBottomPotlView.xaml 的交互逻辑
    /// </summary>
    public partial class FlyBottomPotlView : UserControl
    {
        public FlyBottomPotlView()
        {
            InitializeComponent();
            rpViewModel = ReaderParamsViewModel.GetReaderParamsViewModel();
            DataContext = rpViewModel;
        }
        ReaderParamsViewModel rpViewModel;

        public void MsgLogHandler(string outmsg)
        {
            rpViewModel.PotlString += outmsg;
        }
        private void btnClear_Click(object sender, RoutedEventArgs e)
        {
            rpViewModel.PotlString = "";
        }

        private void IsPotlPrint_Toggled(object sender, RoutedEventArgs e)
        {
            if (tsIsPotlPrint.IsOn)
                Reader.ProtocolPrinter = new Reader.SerialCommOutput(MsgLogHandler);
            else
                Reader.ProtocolPrinter = null;
        }
    }
}
