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
using System.Windows.Shapes;
using ReaderManager.Models;

namespace ReaderManager
{
    /// <summary>
    /// SuccessWnd.xaml 的交互逻辑
    /// </summary>
    public partial class SuccessWnd : Window
    {
        public SuccessWnd(string tip)
        {
            InitializeComponent();
            lbTip.Content = tip;
            root = ReaderParamsViewModel.GetMainWindow();
            Left = root.Left + (root.Width - Width) / 2;
            Top = root.Top+ (root.Height - Height) / 2;
        }
        MainWindow root = null;
    }
}
