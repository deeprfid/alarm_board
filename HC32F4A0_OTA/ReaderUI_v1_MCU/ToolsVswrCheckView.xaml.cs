using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
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
using LiveCharts;
using LiveCharts.Wpf;
using ModuleTech;
using ReaderManager.Models;

namespace ReaderManager
{
    /// <summary>
    /// ToolsVswrCheckView.xaml 的交互逻辑
    /// </summary>
    public partial class ToolsVswrCheckView : UserControl
    {
        public ToolsVswrCheckView()
        {
            InitializeComponent();
            rpViewModel = ReaderParamsViewModel.GetReaderParamsViewModel();
            rootWnd = ReaderParamsViewModel.GetMainWindow();
            DataContext = rpViewModel;
            ccStickChart.AxisX[0].Labels = xlabelslist;
        }
        public ObservableCollection<string> xlabelslist = new ObservableCollection<string>();
        MainWindow rootWnd = null;
        ReaderParamsViewModel rpViewModel = null;

        public class Fre_Vswr
        {
            public int Fre { get; set; }
            public float Vswr { get; set; }
        }
        public class Fre_VswrComper : IComparer<Fre_Vswr>
        {
            public int Compare(Fre_Vswr x, Fre_Vswr y)
            {
                return x.Fre.CompareTo(y.Fre);
            }
        }
        private void btnGet_Click(object sender, RoutedEventArgs e)
        {
            if (rpViewModel.VswrCheckAnt == -1)
            {
                rootWnd.TipMessage(LangResouorce.GetText("Msg_PlsSelect") + LangResouorce.GetText("Msg_Space") +
                     LangResouorce.GetText("ToolsView_vswrcheck_checkant"));
                return;
            }

            int power = -1;
            if (ValidatioAlgorithm.validNumberParam(rpViewModel.VswrCheckPower,
                "ToolsView_vswrcheck_txpower", rpViewModel.MinTxPower, rpViewModel.MaxTxPower,
                out power) != 0)
                return;
            try
            {
                VswrQueryParam vqp = new VswrQueryParam();
                vqp.Power = (ushort)power;
                vqp.AntId = (byte)(rpViewModel.VswrCheckAnt+1);
                vqp.Rg = (Region)rpViewModel.ModReader.ParamGet("Region");
                rpViewModel.ModReader.ParamSet("AntPowerVswr", vqp);

                Dictionary<int, int> antvs = (Dictionary<int, int>)rpViewModel.ModReader.ParamGet("AntPowerVswr");
                List<Fre_Vswr> vswrlist = new List<Fre_Vswr>();
                foreach (int fre in antvs.Keys)
                {
                    Fre_Vswr tmp = new Fre_Vswr();
                    tmp.Fre = fre;
                    float rl = (float)Math.Pow((double)10, (double)(((float)antvs[fre] / (float)10) / (float)20));
                    tmp.Vswr = (1 + rl) / (rl - 1);
                    vswrlist.Add(tmp);
                }
                vswrlist.Sort(new Fre_VswrComper());

                ccStickChart.Series.Clear();
                foreach (Fre_Vswr fv in vswrlist)
                {
                    ColumnSeries rs = new ColumnSeries();
                    if (fv.Vswr <= 1.5)
                        rs.Fill = Brushes.Green;
                    else if (fv.Vswr > 1.5 && fv.Vswr <= 3.0)
                        rs.Fill = Brushes.GreenYellow;
                    else if (fv.Vswr > 3.0 && fv.Vswr <= 5.9)
                        rs.Fill = Brushes.Yellow;
                    else
                        rs.Fill = Brushes.Red;
                    rs.Values = new ChartValues<float>() { fv.Vswr };
                    rs.Title = fv.Fre.ToString();
                    ccStickChart.Series.Add(rs);
                }
                rootWnd.TipSuccess(LangResouorce.GetText("Msg_TagOp_TagOpOK"));
            }
            catch (Exception ex)
            {
                rootWnd.TipApiFailed(LangResouorce.GetText("Msg_TagOp_TagOpFailed"), ex);
            }
        }

        private void UserControl_IsVisibleChanged(object sender, DependencyPropertyChangedEventArgs e)
        {
            if ((bool)e.NewValue == true)
            {
                if (rpViewModel.IsConnect)
                {
                    List<string> opants = new List<string>();
                    for (int i = 0; i < rpViewModel.AntPortNumber; ++i)
                        opants.Add((i + 1).ToString());
                    cbbOpAnt.ItemsSource = opants;
                }
            }
        }

        private void UserControl_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            //Debug.WriteLine("this.Width:" + rootWnd.ActualWidth + " userCotlSize:" + mainWndSize);
            foreach (RowDefinition rf in gdMain.RowDefinitions)
            {
                if ((string)rf.Tag != "NoChange")
                    rf.Height = new GridLength(rf.MinHeight * (rootWnd.ActualHeight / rootWnd.mainWndHeightSize));
            }
        }

        private void UserControl_Loaded(object sender, RoutedEventArgs e)
        {
            foreach (RowDefinition rf in gdMain.RowDefinitions)
            {
                if ((string)rf.Tag != "NoChange")
                    rf.Height = new GridLength(rf.MinHeight * (rootWnd.ActualHeight / rootWnd.mainWndHeightSize));
            }
        }
    }
}
