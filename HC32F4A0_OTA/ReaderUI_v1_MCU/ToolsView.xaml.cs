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

namespace ReaderManager
{
    /// <summary>
    /// ToolsView.xaml 的交互逻辑
    /// </summary>
    public partial class ToolsView : UserControl
    {
        public ToolsView()
        {
            InitializeComponent();
            
        }

        private void UserControl_Loaded(object sender, RoutedEventArgs e)
        {
            Debug.WriteLine("tcParamMuem:" + tcParamMuem.ActualWidth + " " + tcParamMuem.ActualHeight);
        }
    }
}
