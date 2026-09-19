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

namespace ReaderManager
{
    /// <summary>
    /// FlyBottomTagFilterView.xaml 的交互逻辑
    /// </summary>
    public partial class FlyBottomTagFilterView : UserControl
    {
        public FlyBottomTagFilterView()
        {
            InitializeComponent();
            rootWnd = ReaderParamsViewModel.GetMainWindow();
            tfModel = ReaderParamsViewModel.GetReaderParamsViewModel().tfViewModel;
            DataContext = tfModel;
            CtrlListSources.AddComboBox(cbbMaskFormat);
            CtrlListSources.AddComboBox(cbbMatchMode);
            /*
            Binding binding = new Binding();
            binding.Path = new PropertyPath("Mask");
            TagFilter_Mask_FormatValidation rule = new TagFilter_Mask_FormatValidation();
            rule.tfVM = tfModel;
            rule.ValidatesOnTargetUpdated = true;
            binding.ValidationRules.Add(rule);
            tbMask.SetBinding(TextBox.TextProperty, binding);
            */
            //tbMask.SetBinding
        }

        TagFilterViewModel tfModel = null;
        MainWindow rootWnd = null;
        private void cbIsUseInv_Click(object sender, RoutedEventArgs e)
        {
            if ((bool)cbIsUseInv.IsChecked)
            {
                string ret = tfModel.validParams();
                if (ret != "ok")
                {
                    rootWnd.TipMessage(ret);
                    tfModel.IsUseInv = false;
                }
            }
        }

        private void cbIsUseTagOp_Click(object sender, RoutedEventArgs e)
        {
            if ((bool)cbIsUseTagOp.IsChecked)
            {
                string ret = tfModel.validParams();
                if (ret != "ok")
                {
                    rootWnd.TipMessage(ret);
                    tfModel.IsUseTagOp = false;
                }
            }
        }

        private void cbbMaskFormat_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            tfModel.IsUseTagOp = false;
            tfModel.IsUseInv = false;
        }

        private void Textbox_TextChanged(object sender, TextChangedEventArgs e)
        {
            tfModel.IsUseTagOp = false;
            tfModel.IsUseInv = false;
        }

        private void ComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if(tfModel.Bank==0)
                tfModel.StartAddr = "32";
            else if(tfModel.Bank == 1)
                tfModel.StartAddr = "0";
            else if(tfModel.Bank == 2)
                tfModel.StartAddr = "0";

            StartSite.Text = tfModel.StartAddr;
            cbbMatchMode.SelectedIndex = 0;
            cbbMaskFormat.SelectedIndex = 1;
        }
    }

    
}
