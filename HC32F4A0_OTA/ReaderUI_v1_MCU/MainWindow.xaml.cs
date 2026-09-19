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
//using System.Windows.Shapes;
using MahApps.Metro.Controls;
using MahApps.Metro.Controls.Dialogs;
using System.Diagnostics;
using ReaderManager.Models;
using System.Configuration;
using System.IO;
using System.Threading;

namespace ReaderManager
{
    /// <summary>
    /// MainWindow.xaml 的交互逻辑
    /// </summary>
    public partial class MainWindow : MetroWindow
    {
        public Dictionary<string, object> AllViews = new Dictionary<string, object>();

        public int txtFilesInt = 0;
        public MainWindow()
        {
            InitializeComponent();
            string appPath = AppDomain.CurrentDomain.BaseDirectory;
            var language = Thread.CurrentThread.CurrentCulture.Name;

            if (File.Exists(appPath + @"\Lang\" + language + ".xaml")) 
                LangSwitcher(appPath + @"\Lang\" + language + ".xaml");
            else if (!IsChineseSimple())
                 LangSwitcher(appPath + @"\Lang\En.xaml");
        }

        private void LaunchGitHubSite(object sender, RoutedEventArgs e)
        {

        }

        static public void EnumVisual(Visual myVisual)
        {
            for (int i = 0; i < VisualTreeHelper.GetChildrenCount(myVisual); i++)
            {
                // Retrieve child visual at specified index value.
                Visual childVisual = (Visual)VisualTreeHelper.GetChild(myVisual, i);

                // Do processing of the child visual object.

                // Enumerate children of the child visual object.
                
                if (childVisual.GetType() == typeof(TextBlock))
                {
                    TextBlock tmp = (TextBlock)childVisual;
                        tmp.FontWeight = FontWeights.Bold;
                }

                EnumVisual(childVisual);
            }
        }
        public void disableInvFlyBtn()
        {
            btnOpenFlyBottomInv.IsEnabled = false;
            this.flyBottomInv.IsOpen = false;
        }

        public void enableInvFlyBtn()
        {
            btnOpenFlyBottomInv.IsEnabled = true;
        }

        public void btnChangeLang_Click(object sender, RoutedEventArgs e)
        {
            string langsource = "";
            string appPath = AppDomain.CurrentDomain.BaseDirectory;
            string[] txtFiles = Directory.GetFiles(appPath + @"\Lang", "*.xaml");

            string fileName = Path.GetFileName(txtFiles[txtFilesInt]);
            langsource = "Lang\\" + fileName;
            txtFilesInt++;
            if (txtFilesInt == txtFiles.Length)
                txtFilesInt = 0;

            Console.WriteLine(appPath + langsource);
            LangSwitcher(appPath + langsource);
        }

        public void LangSwitcher(string name) {

            CtrlListSources.BeforeSetLang();
            int DicCnt = Application.Current.Resources.MergedDictionaries.Count;
            
            ResourceDictionary dict = new ResourceDictionary();
            dict.Source = new Uri(name, UriKind.Absolute);
            Application.Current.Resources.MergedDictionaries.RemoveAt(DicCnt - 1);
            Application.Current.Resources.MergedDictionaries.Add(dict);

            ((ParamAntsView)AllViews["ParamAntsView"]).SwitchLanguage();
            rdpmViewModel.SwitchLanguage();
            if (AllViews.ContainsKey("ToolsDevSearchView") && AllViews["ToolsDevSearchView"] is ToolsDevSearchView tsv)
                tsv.UpdateColumnHeaders();
          //  CtrlListSources.SetLang();
            CtrlListSources.RestoreIndices();
        }


        public async void TipMessage(string tip)
        {
            MetroDialogSettings mds = new MetroDialogSettings();
            mds.DialogTitleFontSize = 19;
            mds.DialogMessageFontSize = 15;

            await this.ShowMessageAsync(LangResouorce.GetText("Msg_Title"), tip, MessageDialogStyle.Affirmative, mds);
        }
        
        public async void TipApiFailed(string opdesc, Exception ex)
        {
            MetroDialogSettings mds = new MetroDialogSettings();
            mds.DialogTitleFontSize = 19;
            mds.DialogMessageFontSize = 15;
            string tip = opdesc + LangResouorce.GetText("Msg_Comma") +
             LangResouorce.GetText("Msg_ErrInfo") +
             LangResouorce.GetText("Msg_Colon") + ex.ToString();
            await this.ShowMessageAsync(LangResouorce.GetText("Msg_Title"), tip, MessageDialogStyle.Affirmative, mds);
        }

        public void TipSuccess(string tip)
        {
            SuccessWnd wnd = new SuccessWnd(tip);
            wnd.ShowDialog();
        }

        private void MetroWindow_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            if (AllViews.ContainsKey("InventoryView"))
            {
                InventoryView iv = (InventoryView)AllViews["InventoryView"];
                iv.btnStop_Click(null, null);
            }
        }

        bool IsGetuserCotlSize = false;
        public double mainWndSize;
        public double mainWndHeightSize;
        internal ReaderParamsViewModel rdpmViewModel = new ReaderParamsViewModel();
        private void MetroWindow_Loaded(object sender, RoutedEventArgs e)
        {
            if (!IsGetuserCotlSize)
            {
                mainWndSize = ActualWidth;
                mainWndHeightSize = ActualHeight;
                IsGetuserCotlSize = true;
            }
        }

        private void btnOpenFlyBottomPotl_Click(object sender, RoutedEventArgs e)
        {           
            this.flyBottomInv.IsOpen = false;
            this.flyBottomTagFilter.IsOpen = false;
            if (this.flyBottomPotl.IsOpen)
                this.flyBottomPotl.IsOpen = false;
            else { 
                this.flyBottomPotl.IsOpen = true;
            }
        }

        private void btnOpenFlyBottomInv_Click(object sender, RoutedEventArgs e)
        {
            this.flyBottomPotl.IsOpen = false;           
            this.flyBottomTagFilter.IsOpen = false;
            if (this.flyBottomInv.IsOpen)
                this.flyBottomInv.IsOpen = false;
            else {
                this.flyBottomInv.IsOpen = true;
                var s = ReaderParamsViewModel.GetMainWindow();
                ((FlyBottomInvView)ReaderParamsViewModel.GetMainWindow().AllViews["FlyBottomInvView"]).Refresh();
            }
        }

        private void btnOpenFlyBottomTf_Click(object sender, RoutedEventArgs e)
        {
            this.flyBottomPotl.IsOpen = false;
            this.flyBottomInv.IsOpen = false;
            if (this.flyBottomTagFilter.IsOpen)
                this.flyBottomTagFilter.IsOpen = false;
            else
                this.flyBottomTagFilter.IsOpen = true;
        }

        public static bool IsChineseSimple()
        {
            return Thread.CurrentThread.CurrentCulture.Name == "zh-CN";
        }
    }
}
