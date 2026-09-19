// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

using System.Windows;
using System.Windows.Controls;
using MahApps.Metro.Controls;
using MahApps.Metro.Controls.Dialogs;
using System.Diagnostics;
using System.Data;
using System.Linq;
using System.Windows.Input;
using System.Threading;
using System;
using System.Collections.Generic;
using System.Windows.Media;
using System.Collections.ObjectModel;
using ReaderManager.Models;
using System.Windows.Data;
using System.ComponentModel;
using ModuleTech;
//using LiveCharts;
//using LiveCharts.Wpf;
using System.Threading.Tasks;
using System.Windows.Threading;
using System.Windows.Controls.Primitives;
using System.Runtime.InteropServices;
using Microsoft.Office.Interop.Excel;
using System.Timers;

using System.IO;

namespace ReaderManager
{
    /// <summary>
    /// Interaction logic for AboutView.xaml
    /// </summary>
    public partial class InventoryView : UserControl
    {
        DispatcherTimer delayTimer = null;

        MainWindow rootWnd = null;
        ReaderParamsViewModel rpViewModel = null;
        public InventoryView()
        {
            InitializeComponent();

            rpViewModel = ReaderParamsViewModel.GetReaderParamsViewModel();
            rootWnd = ReaderParamsViewModel.GetMainWindow();
            rootWnd.AllViews["InventoryView"] = this;
            dgTags.ItemsSource = rpViewModel.listTags;
            rpViewModel.workTime = 0;
            //tbTagTime.DataContext = rpViewModel;
            DataContext = rpViewModel;
             
            
            /*
            ColumnSeries mylineseries = new ColumnSeries();
            mylineseries.Values = new ChartValues<int> { 100 };
            ccInvSpeed.Series.Add(mylineseries);
            linestart();
            */
        }
        /*
        public void linestart()
        {
            Task.Run(() =>
            {
                var r = new Random();
                while (true)
                {
                    Thread.Sleep(200);
     
                    //通过Dispatcher在工作线程中更新窗体的UI元素
                    Dispatcher.Invoke(() =>
                    {
                        //更新横坐标时间
                        //Labels.Add(DateTime.Now.ToString());
                        //Labels.RemoveAt(0);
                        //更新纵坐标数据

                        ccInvSpeed.Series[0].Values = new ChartValues<int> { r.Next() % 800 }; ;

                    });
                }
            });
        }
        */
       
        public List<FlyBottomInvView.FreqTagCount> GetFreqTagCntList()
        {
            return statFreqDic.Values.ToList();
        }

        public List<FlyBottomInvView.AntTagCount> GetAntTagCntList()
        {
            return statAntDic.Values.ToList();
        }

        Dictionary<int, FlyBottomInvView.FreqTagCount> statFreqDic = new Dictionary<int, FlyBottomInvView.FreqTagCount>();
        Dictionary<int, FlyBottomInvView.AntTagCount> statAntDic = new Dictionary<int, FlyBottomInvView.AntTagCount>();

        List<AntInfo> IsUseInvAnts = new List<AntInfo>();
        List<TagReadData> tagList = new List<TagReadData>();

        int Ant5300 = 0;
        int antForTime = 0;

        delegate void UpdateListview(TagReadData[] tags);
        public void AddTagsToList(TagReadData[] tags)
        {
            foreach (TagReadData tag in tags)
            {
                tagList.Add(tag);
                ////////////
                statFreqDic[tag.Frequency].tagcnt += tag.ReadCount;
                statAntDic[rpViewModel.Is5300or3500 ? IsUseInvAnts[Ant5300].AntId : tag.Antenna].tagcnt += tag.ReadCount;
                ////////////
                string tagkey = tag.EPCString;
                if (rpViewModel.UniByAnt)
                    tagkey += tag.Antenna.ToString();
                if (rpViewModel.UniByBankData)
                    tagkey += tag.EMDDataString;

                if (!rpViewModel.dicUniTags.ContainsKey(tagkey))
                {
                    TagInfo ti = new TagInfo();
                    ti.Epc = tag.EPCString;
                    ti.Freq = tag.Frequency;
                    ti.Reads = tag.ReadCount;
                    ti.Ant = rpViewModel.Is5300or3500 ? IsUseInvAnts[Ant5300].AntId : tag.Antenna;
                    ti.Phase = PhaseTostring(tag.Phase, tag.CRC);
                    ti.Prot = "Gen2";
                    ti.BankData = tag.EMDDataString;
                    ti.Rssi = tag.Rssi;

                    rpViewModel.listTags.Add(ti);
                    rpViewModel.dicUniTags.Add(tagkey, rpViewModel.listTags.Count - 1);
                }
                else
                {
                    int index = rpViewModel.dicUniTags[tagkey];
                    rpViewModel.listTags[index].Reads += tag.ReadCount;
                    rpViewModel.listTags[index].Ant = rpViewModel.Is5300or3500 ? IsUseInvAnts[Ant5300].AntId : tag.Antenna;
                    rpViewModel.listTags[index].BankData = tag.EMDDataString;
                    rpViewModel.listTags[index].Freq = tag.Frequency;
                    rpViewModel.listTags[index].Phase = PhaseTostring(tag.Phase,tag.CRC);
                    rpViewModel.listTags[index].Rssi = tag.Rssi;
                }
            }
            rpViewModel.listTagsReadCount = rpViewModel.listTags.Sum(r => r.Reads);
            tbTagReadCount.Content = rpViewModel.listTagsReadCount;
        }

        public static System.Threading.Timer timer;
        public static bool canExecute = true;

        public string PhaseTostring(int phase,int crc) {
            if (rpViewModel.ModReader.HwDetails.module.ToString().IndexOf("SIM") != -1)
            {
                float xw1 = (float)(((crc & 0x0000ffff)) / 4096.0 * 360);
                float xw2 = (float)((phase & 0x0000ffff) / 4096.0 * 360);

                //float xw1 = (float)(((tag.crc & 0x0000ffff)));
                //float xw2 = (float)((tag.Phase & 0x0000ffff));

                xw1 = phase == 0 ? 0 : xw1;
                return Math.Round(xw1, 2).ToString() + "/" + Math.Round(xw2, 2).ToString();
            }
            else {
                float xw = (phase & 0x3f) * 180 / 64;
                return Math.Round(xw, 2).ToString();
            }
        }
       public  void EnableExecution(object state)
        {
            rpViewModel.ModReader.GPOSet(5, true);
            Thread.Sleep(rpViewModel.IsGpi5Time);
            rpViewModel.ModReader.GPOSet(5, false);
            // 在定时器完成后，设置为可执行状态
            canExecute = true;
        }

        public void OnTagRead(object sender, Reader.TagsReadEventArgs tagsArgs)
        {
            Console.WriteLine("回调时间:" + DateTime.Now.ToString());
            Dispatcher.BeginInvoke(new UpdateListview(AddTagsToList), new object[] { tagsArgs.Tags });

            if (rpViewModel.IsGpi5) { 
                if(timer == null)
                    timer = new System.Threading.Timer(EnableExecution, null, Timeout.Infinite, Timeout.Infinite);

                if (!canExecute)
                    return;
                Console.WriteLine("响应时间:"+DateTime.Now.ToString());
                canExecute = false;
                timer.Change(0, 0);
            }
        }

        private void ChangeDgColumnsWidth()
        {
            int pos = 0;
            double totWidth;

            dgTags.Width = this.ActualWidth - 35;
           if (this.ActualWidth - 35 <= dgTags.MinWidth)
                totWidth = dgTags.MinWidth - 15;
           else
                totWidth = this.ActualWidth - 35 -15;

            Debug.WriteLine("this.ActualWidth:" + this.ActualWidth);

            gvTags.Columns[pos++].Width = totWidth * 0.336;
            gvTags.Columns[pos++].Width = totWidth * 0.093;
            gvTags.Columns[pos++].Width = totWidth * 0.073;
            gvTags.Columns[pos++].Width = totWidth * 0.175;
            gvTags.Columns[pos++].Width = totWidth * 0.080;
            gvTags.Columns[pos++].Width = totWidth * 0.073;
            gvTags.Columns[pos++].Width = totWidth * 0.08;
            gvTags.Columns[pos++].Width = totWidth * 0.080;
        }
        private void UserControl_Loaded(object sender, RoutedEventArgs e)
        {
            tbTagTime.Content = rpViewModel.workTime;
            dgTags.ItemsSource = rpViewModel.listTags;
            tbTagReadCount.Content = rpViewModel.listTagsReadCount;
            ChangeDgColumnsWidth();
        }

        private void UserControl_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            ChangeDgColumnsWidth();
        }

        public void btnStop_Click(object sender, RoutedEventArgs e)
        {
            btnStopClick();
        }

        //停止盘点
        public void btnStopClick() {
            if (!rpViewModel.IsInventory)
                return;

            if (delayTimer != null)
                delayTimer.Stop();

            try
            {
                rpViewModel.ModReader.StopReading();
                rpViewModel.IsInventory = false;
                rpViewModel.InventoryErrLog = "";

            }
            catch (Exception exp)
            {
                rootWnd.TipApiFailed(LangResouorce.GetText("Msg_Inventory_StopInvFailed"), exp);
                return;
            }
        }

        //public delegate void InvolkUiBtnHandler(object sender, RoutedEventArgs e);
        public void btnStart_Click(object sender, RoutedEventArgs e)
        {
            rpViewModel.dicUniTags.Clear();
            rpViewModel.listTags.Clear();
            rpViewModel.listTagsReadCount = 0;
            btnStartClick();
        }

        //开始盘点
        public void btnStartClick(bool IsworkTime=true) {
            try
            {
                BackReadOption bro = new BackReadOption();
                bro.IsFastRead = rpViewModel.IsFastInvMode;

                var authentication = rpViewModel.HardwareVer.Split('.')[2];
                if (rpViewModel.getQuickModeType == 3&& CertificationCountry(authentication))
                    rpViewModel.ModReader.ParamSet("Ex10FastModeParams", new Ex10FastModeParams());
                else 
                    rpViewModel.ModReader.ParamSet("Ex10FastModeParams", null);

                bro.ReadDuration = ushort.Parse(rpViewModel.ReadDur);
                bro.ReadInterval = uint.Parse(rpViewModel.SleepDur);
                BackReadOption.FastReadTagMetaData frtmd = new BackReadOption.FastReadTagMetaData();
                frtmd.IsEmdData = true;
                frtmd.IsFrequency = true;
                frtmd.IsReadCnt = true;
                frtmd.IsRSSI = true;
                frtmd.IsTimestamp = true;
                frtmd.IsAntennaID = true;
                frtmd.IsRFU = true;

                bro.FRTMetadata = frtmd;
                List<int> invants = new List<int>();

                //如果是单天线设备且没有勾选天线则默认1号天线
                IsUseInvAnts = rpViewModel.ocListAnts.Where(r => r.IsUseInv == true).ToList();
                if (IsUseInvAnts.Count == 0&& rpViewModel.AntPortNumber == 1)
                {
                    AntPower[] apwrs = (AntPower[])rpViewModel.ModReader.ParamGet("AntPowerConf");
                    AntInfo ant = new AntInfo();
                    ant.AntId =  1;
                    ant.Name = LangResouorce.GetText("ParamSettings_ParamAnts_AntInfo_Name");
                    ant.Name += apwrs[0].AntId.ToString();
                    ant.ReadPower =  apwrs[0].ReadPower;
                    ant.WritePower =apwrs[0].WritePower;
                    ant.IsUseInv = true;
                    rpViewModel.ocListAnts.Add(ant);
                    IsUseInvAnts = rpViewModel.ocListAnts.Where(r => r.IsUseInv == true).ToList();
                }

                if (rpViewModel.Is5300or3500) {

                    invants.Add(1);
                    Ant5300 = 0;

                    for (int i = 0; i < IsUseInvAnts.Count; i++)
                        Is5300or3500Ant(IsUseInvAnts[i].AntId);
                    
                    if (rpViewModel.EnableAntMaxDwellTime&& rpViewModel.AntMaxDwellTime!="")
                        antForTime =Convert.ToInt32(rpViewModel.AntMaxDwellTime);
                }
                 else if (rpViewModel.ocListAnts.Count > 0)
                {
                    for (int i = 0; i < rpViewModel.ocListAnts.Count - 1; ++i)
                    {
                        if (rpViewModel.ocListAnts[i].IsUseInv)
                            invants.Add(rpViewModel.ocListAnts[i].AntId);
                    }
                }

                //rpViewModel.HardwareVer
                statAntDic.Clear();
                foreach (var at in IsUseInvAnts)
                {
                    FlyBottomInvView.AntTagCount atc = new FlyBottomInvView.AntTagCount();
                    atc.ant = at.AntId;
                    atc.tagcnt = 0;
                    statAntDic.Add(at.AntId, atc);
                }

                int[] hoptab = (int[])rpViewModel.ModReader.ParamGet("FrequencyHopTable");
                statFreqDic.Clear();
                foreach (int freq in hoptab)
                {
                    FlyBottomInvView.FreqTagCount frc = new FlyBottomInvView.FreqTagCount();
                    frc.freq = freq;
                    frc.tagcnt = 0;
                    statFreqDic.Add(freq, frc);
                }
                rpViewModel.ModReader.ParamSet("BackReadOption", bro);
                rpViewModel.ModReader.ParamSet("ReadPlan", new SimpleReadPlan(TagProtocol.GEN2, invants.ToArray(), 30));
                rpViewModel.ModReader.ParamSet("Singulation", rpViewModel.tfViewModel.GetTagFilter(1));

                if (IsworkTime) {
                    rpViewModel.StopTime = Environment.TickCount;
                    rpViewModel.workTime = 0;
                }

                rpViewModel.ModReader.StartReading();
                rpViewModel.IsInventory = true;
                delayTimer = new DispatcherTimer();
                delayTimer.Interval = TimeSpan.FromMilliseconds(10);
                delayTimer.Tick += delayTimer_Tick;
                delayTimer.Start();
                
            }
            catch (System.Exception ex)
            {
                if (delayTimer != null)
                    delayTimer.Stop();

                rootWnd.TipApiFailed(LangResouorce.GetText("Msg_Inventory_StartInvFailed"), ex);
                return;
            }
        }

        private void btnClear_Click(object sender, RoutedEventArgs e)
        {
            rpViewModel.listTags.Clear();
            rpViewModel.dicUniTags.Clear();
            tbTagReadCount.Content=rpViewModel.listTagsReadCount=0;
            tbTagTime.Content=rpViewModel.workTime = 0;
        }

        private void btnExport_Click(object sender, RoutedEventArgs e)
        {
            string filename = null;
            System.Windows.Forms.SaveFileDialog sfd = new System.Windows.Forms.SaveFileDialog();
            sfd.Filter = "csv(*.csv)|*.csv|txt(*.txt)| *.txt";
            //sfd.r

            if (sfd.ShowDialog() == System.Windows.Forms.DialogResult.OK)
            {
                filename = sfd.FileName;
                FileInfo fileInfo = new FileInfo(filename);
                fileInfo.Delete();
                StreamWriter streamWriter = fileInfo.CreateText();
                string headline = " EPC,读取次数, 天线,附加数据,协议,RSSI,频率,相位";
                streamWriter.WriteLine(headline);

                string wline = "";
                foreach (TagInfo viewitem in rpViewModel.listTags)
                {
                    wline = "\t" + viewitem.Epc + "," + viewitem.Reads + "," +
                       viewitem.Ant + ",\t" + viewitem.BankData + ",GEN2,"
                       + viewitem.Rssi + "," + viewitem.Freq + ",\t"
                       + viewitem.Phase;
                    streamWriter.WriteLine(wline);
                }
                streamWriter.Flush();
                streamWriter.Close();
            }
            //var title = "盘点数据";
            //try
            //{
            //    //创建Excel
            //    Microsoft.Office.Interop.Excel.Application ExcelApp = new Microsoft.Office.Interop.Excel.Application();
            //    Microsoft.Office.Interop.Excel.Workbook ExcelBook = ExcelApp.Workbooks.Add(System.Type.Missing);
            //    //创建工作表（即Excel里的子表sheet） 1表示在子表sheet1里进行数据导出
            //    Microsoft.Office.Interop.Excel.Worksheet ExcelSheet = (Microsoft.Office.Interop.Excel.Worksheet)ExcelBook.Worksheets[1];
            //    //如果数据中存在数字类型 可以让它变文本格式显示
            //    ExcelSheet.Cells.NumberFormat = "@";
            //    //设置工作表名
            //    ExcelSheet.Name = title;
            //    //设置Sheet标题
            //    string start = "A1";
            //    string end = ChangeASC(8) + "1";
            //    Microsoft.Office.Interop.Excel.Range _Range = (Microsoft.Office.Interop.Excel.Range)ExcelSheet.get_Range(start, end);
            //    _Range.Merge(0);                     //单元格合并动作(要配合上面的get_Range()进行设计)
            //    _Range = (Microsoft.Office.Interop.Excel.Range)ExcelSheet.get_Range(start, end);
            //    _Range.HorizontalAlignment = Microsoft.Office.Interop.Excel.XlHAlign.xlHAlignCenter;
            //    _Range.Font.Size = 22; //设置字体大小
            //    _Range.Font.Name = "宋体"; //设置字体的种类 
            //    ExcelSheet.Cells[1, 1] = title;    //Excel单元格赋值
            //    _Range.EntireColumn.AutoFit(); //自动调整列宽
            //    //写表头
            //    ExcelSheet.Cells[2, 1] ="EPC";
            //    ExcelSheet.Cells[2, 2] = "读次数";
            //    ExcelSheet.Cells[2, 3] = "天线";
            //    ExcelSheet.Cells[2, 4] ="BANT数据";
            //    ExcelSheet.Cells[2, 5] ="协议";
            //    ExcelSheet.Cells[2, 6] ="RSSI";
            //    ExcelSheet.Cells[2, 7] ="频率";
            //    ExcelSheet.Cells[2, 8] = "相位";
            //    start = "A2";
            //    end = ChangeASC(rpViewModel.listTags.Count) + "2";
            //    _Range = (Microsoft.Office.Interop.Excel.Range)ExcelSheet.get_Range(start, end);
            //    _Range.Font.Size = 14; //设置字体大小
            //    _Range.Font.Name = "宋体"; //设置字体的种类  
            //    _Range.EntireColumn.AutoFit(); //自动调整列宽 
            //    _Range.HorizontalAlignment = Microsoft.Office.Interop.Excel.XlHAlign.xlHAlignCenter;
            //    //写数据
            //    for (int i = 0; i < rpViewModel.listTags.Count; i++)
            //    {
            //        ExcelSheet.Cells[i + 3, 1] = rpViewModel.listTags[i].Epc;
            //        ExcelSheet.Cells[i + 3, 2] = rpViewModel.listTags[i].Reads;
            //        ExcelSheet.Cells[i + 3, 3] = rpViewModel.listTags[i].Ant;
            //        ExcelSheet.Cells[i + 3, 4] = rpViewModel.listTags[i].BankData;
            //        ExcelSheet.Cells[i + 3, 5] = rpViewModel.listTags[i].Prot;
            //        ExcelSheet.Cells[i + 3, 6] = rpViewModel.listTags[i].Rssi;
            //        ExcelSheet.Cells[i + 3, 7] = rpViewModel.listTags[i].Freq;
            //        ExcelSheet.Cells[i + 3, 8] = rpViewModel.listTags[i].Phase;
            //    }
            //    //表格属性设置
            //    for (int n = 0; n < rpViewModel.listTags.Count + 1; n++)
            //    {
            //        start = "A" + (n + 3).ToString();
            //        end = ChangeASC(rpViewModel.listTags.Count) + (n + 3).ToString();
            //        //获取Excel多个单元格区域
            //        _Range = (Microsoft.Office.Interop.Excel.Range)ExcelSheet.get_Range(start, end);
            //        _Range.Font.Size = 12; //设置字体大小
            //        _Range.Font.Name = "宋体"; //设置字体的种类
            //        _Range.EntireColumn.AutoFit(); //自动调整列宽
            //        _Range.HorizontalAlignment = Microsoft.Office.Interop.Excel.XlHAlign.xlHAlignCenter; //设置字体在单元格内的对其方式 _Range.EntireColumn.AutoFit(); //自动调整列宽 
            //    }
            //    ExcelApp.DisplayAlerts = false; //保存Excel的时候，不弹出是否保存的窗口直接进行保存 
            //    ////弹出保存对话框,并保存文件
            //    Microsoft.Win32.SaveFileDialog sfd = new Microsoft.Win32.SaveFileDialog();
            //    sfd.DefaultExt = ".csv";
            //    sfd.Filter = "Office 2007 File|*.csv|Office 2000-2003 File|*.csv|所有文件|*.*";
            //    if (sfd.ShowDialog() == true)
            //    {
            //        if (sfd.FileName != "")
            //        {
            //            ExcelBook.SaveAs(sfd.FileName);  //将其进行保存到指定的路径
            //            System.Windows.MessageBox.Show("导出文件已存储为: " + sfd.FileName, "温馨提示");
            //        }
            //    }
            //    //释放可能还没释放的进程
            //    ExcelBook.Close();
            //    ExcelApp.Quit();
            //}
            //catch
            //{

            //}
        }
        
        /// <summary>
        /// 获取当前列列名,并得到EXCEL中对应的列
        /// </summary>
        /// <param name="count"></param>
        /// <returns></returns>
        private string ChangeASC(int count)
        {
            string ascstr = "";
            switch (count)
            {
                case 1:
                    ascstr = "A";
                    break;
                case 2:
                    ascstr = "B";
                    break;
                case 3:
                    ascstr = "C";
                    break;
                case 4:
                    ascstr = "D";
                    break;
                case 5:
                    ascstr = "E";
                    break;
                case 6:
                    ascstr = "F";
                    break;
                case 7:
                    ascstr = "G";
                    break;
                case 8:
                    ascstr = "H";
                    break;
                case 9:
                    ascstr = "I";
                    break;
                case 10:
                    ascstr = "J";
                    break;
                case 11:
                    ascstr = "K";
                    break;
                case 12:
                    ascstr = "L";
                    break;
                case 13:
                    ascstr = "M";
                    break;
                case 14:
                    ascstr = "N";
                    break;
                case 15:
                    ascstr = "O";
                    break;
                case 16:
                    ascstr = "P";
                    break;
                case 17:
                    ascstr = "Q";
                    break;
                case 18:
                    ascstr = "R";
                    break;
                case 19:
                    ascstr = "S";
                    break;
                case 20:
                    ascstr = "T";
                    break;
                default:
                    ascstr = "U";
                    break;
            }
            return ascstr;
        }

        private ListSortDirection _sortDirection;
        private GridViewColumnHeader _sortColumn;
        private void Sort_Click(object sender, RoutedEventArgs e)
        {
            GridViewColumnHeader column = e.OriginalSource as GridViewColumnHeader;
            if (column == null || column.Column == null)
            {
                return;
            }

            if (_sortColumn == column)
            {
                // Toggle sorting direction 
                _sortDirection = _sortDirection == ListSortDirection.Ascending ?
                                                   ListSortDirection.Descending :
                                                   ListSortDirection.Ascending;
            }
            else
            {
                // Remove arrow from previously sorted header 
                if (_sortColumn != null && _sortColumn.Column != null)
                {
                    _sortColumn.Column.HeaderTemplate = null;
                 //   _sortColumn.Column.Width = _sortColumn.ActualWidth - 20;
                }

                _sortColumn = column;
                _sortDirection = ListSortDirection.Ascending;
             //   column.Column.Width = column.ActualWidth + 20;
            }
            /*
            if (_sortDirection == ListSortDirection.Ascending)
            {
                column.Column.HeaderTemplate = Resources["ArrowUp"] as DataTemplate;
            }
            else
            {
                column.Column.HeaderTemplate = Resources["ArrowDown"] as DataTemplate;
            }
            */
            string header = string.Empty;

            // if binding is used and property name doesn't match header content 
            Binding b = _sortColumn.Column.DisplayMemberBinding as Binding;
            if (b != null)
            {
                header = b.Path.Path;
            }

            ICollectionView resultDataView = CollectionViewSource.GetDefaultView(
                                                       (sender as ListView).ItemsSource);
            resultDataView.SortDescriptions.Clear();
            resultDataView.SortDescriptions.Add(
                                        new SortDescription(header, _sortDirection));
        }

        public int CurrentForCount = 0;//当前循环次数
        //计时器触发方法
        void delayTimer_Tick(object sender, EventArgs e)
        {
            if (rpViewModel.StopTime > 0)
            {
                rpViewModel.workTime = Environment.TickCount - rpViewModel.StopTime;
                tbTagTime.Content = rpViewModel.workTime;
            }

             if ((rpViewModel.GetStopTime != 0 && Environment.TickCount >= (rpViewModel.StopTime+ rpViewModel.GetStopTime))
                || (rpViewModel.GetStopCount != 0 && rpViewModel.dicUniTags.Count >= rpViewModel.GetStopCount))
            {
                //循环读取次数加1
                if (rpViewModel.GetForCount != 0 && CurrentForCount < rpViewModel.GetForCount) {
                    CurrentForCount++;

                    //停止盘点
                    btnStopClick();
                    //清空数据
                    rpViewModel.listTags.Clear();
                    rpViewModel.dicUniTags.Clear();
                    //开始盘点
                    btnStartClick();
                    return;
                } 
                CurrentForCount = 0;
                btnStopClick();
            }
            else if (rpViewModel.Is5300or3500&& IsUseInvAnts.Count>1&& antForTime >0) {
                if (Environment.TickCount >= (antForTime + rpViewModel.StopTime)) {

                    //停止盘点
                    btnStopClick();

                    var IsUseInvAnts = rpViewModel.ocListAnts.Where(r => r.IsUseInv == true).ToList();
                    if (Ant5300 == IsUseInvAnts.Count-1)
                        Ant5300 =0;
                    else
                        Ant5300 +=1;

                    Is5300or3500Ant(IsUseInvAnts[Ant5300].AntId);
                    //开始盘点
                    btnStartClick(false);
                }
            }
        }
        
        void TimerHandle(int type) {
            if (type == 0)
            {
                tbTagTime.Content = rpViewModel.workTime;
            }
            else {
                delayTimer.Stop();
                if (!rpViewModel.IsInventory)
                    return;

                try
                {
                    rpViewModel.ModReader.StopReading();
                    rpViewModel.IsInventory = false;
                    rpViewModel.InventoryErrLog = "";
                }
                catch (Exception exp)
                {
                    rootWnd.TipApiFailed(LangResouorce.GetText("Msg_Inventory_StopInvFailed"), exp);
                    return;
                }
            }
        }

        private void DgTags_MouseDoubleClick(object sender, MouseButtonEventArgs e)
        {
            var tagInfo = dgTags.SelectedItem as TagInfo;
            if (tagInfo == null)
                return;

            Clipboard.SetDataObject(tagInfo.Epc);
        }

        private void Is5300or3500Ant(int antId)
        {
            if (antId == 4)
            {
                rpViewModel.ModReader.GPOSet(1, false);
                rpViewModel.ModReader.GPOSet(2, false);
            }
            else if (antId == 3)
            {
                rpViewModel.ModReader.GPOSet(1, true);
                rpViewModel.ModReader.GPOSet(2, false);
            }
            else if (antId == 2)
            {
                rpViewModel.ModReader.GPOSet(1, true);
                rpViewModel.ModReader.GPOSet(2, true);
            }
            else if (antId == 1)
            {
                rpViewModel.ModReader.GPOSet(1, false);
                rpViewModel.ModReader.GPOSet(2, true);
            }
        }

        private bool CertificationCountry(string value) {
            switch (value)
            {
                case "01"://FCC
                case "06"://HK
                case "07"://TAIWAN
                case "04"://KOREA
                case "08"://MALAYSIA
                case "09"://SOUTH_AFRICA
                case "0a"://BRAZIL
                case "0b"://THAILAND
                case "0c"://SINGAPORE
                case "0d"://AUSTRALIA
                case "0f"://URUGUAY
                case "10"://VIETNAM
                case "13"://INDONESIA
                case "14"://NEW_ZEALAND
                case "15"://PERU
                case "A1"://FCC_CUSTOM
                    return false;
                default:
                    return true;
            }
        }

    }
}