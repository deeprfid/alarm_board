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
using ReaderManager.Models;

namespace ReaderManager
{
    /// <summary>
    /// FlyBottonInv.xaml 的交互逻辑
    /// </summary>
    public partial class FlyBottomInvView : UserControl
    {
        MainWindow rootWnd = null;
       
        public FlyBottomInvView()
        {
            InitializeComponent();
            xlabelslist = new ObservableCollection<string>();
            DataContext = this;
            rootWnd = ReaderParamsViewModel.GetMainWindow();
            rootWnd.AllViews["FlyBottomInvView"] = this;
        }

        public ObservableCollection<string> xlabelslist { get; set; }
        private void Button_Click(object sender, RoutedEventArgs e)
        {
            Refresh();
        }
        public class FreqTagCount
        {
            public int freq { get; set; }
            public int tagcnt { get; set; }
            public uint color { get; set; }

        }
        public class AntTagCount
        {
            public int ant { get; set; }
            public int tagcnt { get; set; }
        }
        public void RefreshStickChart(List<FreqTagCount> frclist, List<AntTagCount> antlist)
        {
            frclist.Sort((p1, p2) =>
            {
                return p1.tagcnt.CompareTo(p2.tagcnt);
            });

            for (int i = 0; i < frclist.Count; ++i)
            {
                if ((frclist.Count< 5) )
                    frclist[i].color = 0x500E3193;
                else if(i / (frclist.Count / 5) == 0)
                    frclist[i].color = 0x500E3193;
                else if (i / (frclist.Count / 5) == 1)
                    frclist[i].color = 0x800E3193;
                else if (i / (frclist.Count / 5) == 2)
                    frclist[i].color = 0xA00E3193;
                else if (i / (frclist.Count / 5) == 3)
                    frclist[i].color = 0xB00E3193;
                else if (i / (frclist.Count / 5) == 4)
                    frclist[i].color = 0xFF0E3193;
            }

            frclist.Sort((p1, p2) =>
            {
                return p1.freq.CompareTo(p2.freq);
            });

            ccStickChart.Series.Clear();
            //         ccStickChart.AxisX[0].Labels = xlabelslist;
            //           ccStickChart.AxisX[0].LabelsRotation = 20;
            // ccStickChart.AxisX[0].ShowLabels = true;
            foreach (FreqTagCount frc in frclist)
            {     
                ColumnSeries rs = new ColumnSeries();
            //    rs.DataLabels = true; //0E3193
                rs.Fill = new SolidColorBrush(Color.FromArgb((byte)((frc.color >> 24) & 0xff),
                    (byte)((frc.color >> 16) & 0xff), (byte)((frc.color >> 8) & 0xff),
                    (byte)((frc.color >> 0) & 0xff)));
                rs.Values = new ChartValues<int>() { frc.tagcnt};
                rs.Title = frc.freq.ToString();
                ccStickChart.Series.Add(rs);               
            }
            
            pcPicChart.Series.Clear();
            foreach (AntTagCount atc in antlist)
            {
                PieSeries ps = new PieSeries();
                ps.Values = new ChartValues<int>() { atc.tagcnt };
                ps.Title = "Ant" + atc.ant;
                pcPicChart.Series.Add(ps);
            }
        }

        private void Grid_Loaded(object sender, RoutedEventArgs e)
        {
            Console.WriteLine();
        }

        private void Grid_DataContextChanged(object sender, DependencyPropertyChangedEventArgs e)
        {
            Console.WriteLine();
        }

        public void Refresh() {
            RefreshStickChart(((InventoryView)ReaderParamsViewModel.GetMainWindow().AllViews["InventoryView"]).GetFreqTagCntList(),
                   ((InventoryView)ReaderParamsViewModel.GetMainWindow().AllViews["InventoryView"]).GetAntTagCntList());
        }
    }
}
