using ReaderManager.Models;
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
using System.Diagnostics;
using System.Windows.Controls.Primitives;
using ModuleTech;
using System.Threading;

namespace ReaderManager
{
    /// <summary>
    /// ParamAntsView.xaml 的交互逻辑
    /// </summary>
    public partial class ParamAntsView : UserControl
    {
        
        public ParamAntsView()
        {
            InitializeComponent();
            int DicCnt = Application.Current.Resources.MergedDictionaries.Count;
            rpViewModel = ReaderParamsViewModel.GetReaderParamsViewModel();          
            dgAntsConf.ItemsSource = rpViewModel.ocListAnts;


            rootWnd = ReaderParamsViewModel.GetMainWindow();
            rootWnd.AllViews["ParamAntsView"] = this;
            /*
            Binding binding = new Binding();
            binding.Path = new PropertyPath("ReadPower");
            EditNumberValidation rule = new EditNumberValidation();
            rule.rpModel = rpViewModel;
            rule.vmode = EditNumberValidation.ValidModeCode.NumValid_powerrange;
            rule.ValidatesOnTargetUpdated = true;
            binding.ValidationRules.Add(rule);
            dgtxReadpower.Binding = binding;
            */
        }

        bool HasGotParams = false;
        public void ResetView()
        {
            HasGotParams = false;
        }

        delegate void AddEveHandler();
        void AddCkHandler()
        {
            CheckBox cb = dgAntsConf.Columns[4].GetCellContent(dgAntsConf.Items[rpViewModel.ocListAnts.Count - 1]) as CheckBox;
            if(cb!=null)
                cb.Click += OnCheckBoxClick;
        }
        
        public int SetParams()
        {
            List<AntPower> antspwr = new List<AntPower>();
            for (int i = 0; i < rpViewModel.ocListAnts.Count-1; ++i)
            {
                if (rpViewModel.ocListAnts[i].ReadPower < rpViewModel.MinTxPower ||
                    rpViewModel.ocListAnts[i].ReadPower > rpViewModel.MaxTxPower ||
                    rpViewModel.ocListAnts[i].WritePower < rpViewModel.MinTxPower ||
                    rpViewModel.ocListAnts[i].WritePower > rpViewModel.MaxTxPower)
                {
                    ReaderParamsViewModel.GetMainWindow().TipMessage(LangResouorce.GetText("Msg_Params_PowerErr") + rpViewModel.MinTxPower + "-" + rpViewModel.MaxTxPower);
                    return -1;
                }
                else
                {
                    AntPower ap = new AntPower();
                    ap.AntId = (byte)rpViewModel.ocListAnts[i].AntId;
                    ap.ReadPower = (ushort)rpViewModel.ocListAnts[i].ReadPower;
                    ap.WritePower = (ushort)rpViewModel.ocListAnts[i].WritePower;
                    antspwr.Add(ap);
                }
            }

            try
            {
                if (!rpViewModel.Is5300or3500)
                    rpViewModel.ModReader.ParamSet("AntPowerConf", antspwr.ToArray());
                else
                    rpViewModel.ModReader.ParamSet("AntPowerConf", new AntPower[] { antspwr[0] });

            }
            catch (System.Exception exx)
            {
                ReaderParamsViewModel.GetMainWindow().TipApiFailed(LangResouorce.GetText("Msg_Params_SetFailed"), exx);
                return -1;
            }

            ReaderParamsViewModel.GetMainWindow().TipSuccess(LangResouorce.GetText("Msg_Params_SetSuccess"));                    
            return 0;
        }

        public int SetPerpetualParams() {
            ushort[] vals = new ushort[(rpViewModel.ocListAnts.Count-1) * 3];
            for (int i = 0; i < rpViewModel.ocListAnts.Count - 1; ++i)
            {
                if (rpViewModel.ocListAnts[i].ReadPower < rpViewModel.MinTxPower ||
                    rpViewModel.ocListAnts[i].ReadPower > rpViewModel.MaxTxPower ||
                    rpViewModel.ocListAnts[i].WritePower < rpViewModel.MinTxPower ||
                    rpViewModel.ocListAnts[i].WritePower > rpViewModel.MaxTxPower)
                {
                    ReaderParamsViewModel.GetMainWindow().TipMessage(LangResouorce.GetText("Msg_Params_PowerErr") + rpViewModel.MinTxPower + "-" + rpViewModel.MaxTxPower);
                    return -1;
                }
                else
                {
                    vals[(i * 3)] = (ushort)rpViewModel.ocListAnts[i].ReadPower;
                    vals[(i * 3) + 1] = (ushort)rpViewModel.ocListAnts[i].WritePower;
                    vals[(i * 3) + 2] = 0;
                }
            }

            try
            {
                if(!rpViewModel.Is5300or3500)
                    rpViewModel.ModReader.ParamSet("ModuleSave_Ant_Power_Time", vals);
                else
                    rpViewModel.ModReader.ParamSet("ModuleSave_Ant_Power_Time", new ushort[] { vals[0], vals[1], vals[2] });
            }
            catch (System.Exception exx)
            {
                ReaderParamsViewModel.GetMainWindow().TipApiFailed(LangResouorce.GetText("Msg_Params_SetFailed"), exx);
                return -1;
            }

            ReaderParamsViewModel.GetMainWindow().TipSuccess(LangResouorce.GetText("Msg_Params_SetSuccess"));
            return 0;
        }

        public int GetParams(bool isTip=false)
        {
            try
            {
                AntPower[] apwrs = (AntPower[])rpViewModel.ModReader.ParamGet("AntPowerConf");
                int[] connants = (int[])rpViewModel.ModReader.ParamGet("ConnectedAntennas");
                if (!HasGotParams)
                {
                    rpViewModel.ocListAnts.Clear();
                    for (int i = 0; i < rpViewModel.AntPortNumber; ++i)
                    {
                        AntInfo ant = new AntInfo();
                        ant.AntId = i + 1;
                        ant.Name = LangResouorce.GetText("ParamSettings_ParamAnts_AntInfo_Name");
                        ant.Name += rpViewModel.Is5300or3500 ? (i+1).ToString() : apwrs[i].AntId.ToString();
                        ant.ReadPower = rpViewModel.Is5300or3500 ? apwrs[0].ReadPower: apwrs[i].ReadPower;
                        ant.WritePower = rpViewModel.Is5300or3500 ? apwrs[0].WritePower: apwrs[i].WritePower;

                        ant.IsUseInv = false;
                        ant.ConnState = LangResouorce.GetText("ParamSettings_ParamAnts_AntInfo_Unconnected");
                        for (int j = 0; j < connants.Length; ++j)
                        {
                            bool isfind = false;
                            if (apwrs[i].AntId == connants[j])
                            {
                                ant.ConnState = LangResouorce.GetText("ParamSettings_ParamAnts_AntInfo_Connected");
                                ant.IsUseInv = true;
                                isfind = true;
                                break;
                            }
                            if (!isfind)
                            {
                                ant.IsUseInv = false;
                                ant.ConnState = LangResouorce.GetText("ParamSettings_ParamAnts_AntInfo_Unconnected");
                            }
                        }

                        if(rpViewModel.AntPortNumber == 1)
                            ant.IsUseInv = true;

                        rpViewModel.ocListAnts.Add(ant);
                    }
                    AntInfo allant = new AntInfo();
                    allant.Name = LangResouorce.GetText("ParamSettings_ParamAnts_AntInfo_SetAll");
                    allant.ReadPower = 0;
                    allant.WritePower = 0;
                    allant.ConnState = "";
                    allant.IsUseInv = false;
                    rpViewModel.ocListAnts.Add(allant);

                    new Thread(new ThreadStart(delegate ()
                    {
                        Thread.Sleep(400);
                        Dispatcher.BeginInvoke(new AddEveHandler(AddCkHandler));
                    })).Start();
                }
                else
                {
                    if (rpViewModel.AntPortNumber == 1)
                        rpViewModel.ocListAnts[0].IsUseInv = true;

                    for (int i = 0; i < rpViewModel.AntPortNumber; ++i)
                    {
                        rpViewModel.ocListAnts[i].ReadPower = apwrs[i].ReadPower;
                        rpViewModel.ocListAnts[i].WritePower = apwrs[i].WritePower;
                        for (int j = 0; j < connants.Length; ++j)
                        {
                            bool isfind = false;
                            if (rpViewModel.ocListAnts[i].AntId == connants[j])
                            {
                                rpViewModel.ocListAnts[i].ConnState = LangResouorce.GetText("ParamSettings_ParamAnts_AntInfo_Connected");
                                isfind = true;
                                break;
                            }
                            if (!isfind)
                                rpViewModel.ocListAnts[i].ConnState = LangResouorce.GetText("ParamSettings_ParamAnts_AntInfo_Unconnected");
                        }
                    }
                }

                HasGotParams = true;
                if (isTip)
                    ReaderParamsViewModel.GetMainWindow().TipSuccess(LangResouorce.GetText("Msg_Params_GetSuccess"));
                return 0;
            }
            catch (Exception ex)
            {
                ReaderParamsViewModel.GetMainWindow().TipApiFailed(LangResouorce.GetText("Msg_Params_GetFailed"), ex);
                return -1;
            }
        }

        ReaderParamsViewModel rpViewModel = null;

        private void dgAntsConf_CellEditEnding(object sender, DataGridCellEditEndingEventArgs e)
        {
            if (e.Row.GetIndex() == dgAntsConf.Items.Count-1)
            {
                if (e.Column.DisplayIndex == 1)
                {
                    int rdpower = 0;
                    if (int.TryParse((e.EditingElement as TextBox).Text, out rdpower))
                    {
                        if (rdpower <= rpViewModel.MaxTxPower && rdpower >= rpViewModel.MinTxPower)
                        {
                            for (int i = 0; i < dgAntsConf.Items.Count - 1; ++i)
                                rpViewModel.ocListAnts[i].ReadPower = rdpower;
                        }
                    }
                }
                else if (e.Column.DisplayIndex == 2)
                {
                    int wtpower = 0;
                    if (int.TryParse((e.EditingElement as TextBox).Text, out wtpower))
                    {
                        if (wtpower <= rpViewModel.MaxTxPower && wtpower >= rpViewModel.MinTxPower)
                        {
                            for (int i = 0; i < dgAntsConf.Items.Count - 1; ++i)
                                rpViewModel.ocListAnts[i].WritePower = wtpower;
                        }
                    }
                }
                
            }

        }

        private void OnCheckBoxClick(object sender, RoutedEventArgs e)
        {
            //dgAntsConf.SelectedCells
            // CheckBox cb = (CheckBox)e.Source;
            for (int i = 0; i < rpViewModel.ocListAnts.Count - 1; ++i)
            {
                rpViewModel.ocListAnts[i].IsUseInv = rpViewModel.ocListAnts[rpViewModel.ocListAnts.Count - 1].IsUseInv;
            }
        }

        private void UserControl_SourceUpdated(object sender, DataTransferEventArgs e)
        {
          
        }

        private void UserControl_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            dgAntsConf.Width = dgAntsConf.MinWidth * (rootWnd.ActualWidth / rootWnd.mainWndSize);
        }

        MainWindow rootWnd;
        private void UserControl_Loaded(object sender, RoutedEventArgs e)
        {
            dgAntsConf.Width = dgAntsConf.MinWidth * (rootWnd.ActualWidth / rootWnd.mainWndSize);
        }
        public void SwitchLanguage()
        {
            dgtxName.Header = LangResouorce.GetText("ParamSettings_ParamAnts_datagrid_header_index");
            dgtxReadpower.Header = LangResouorce.GetText("ParamSettings_ParamAnts_datagrid_header_readpower");
            dgtxWritepower.Header = LangResouorce.GetText("ParamSettings_ParamAnts_datagrid_header_writepower");
            dgtxState.Header = LangResouorce.GetText("ParamSettings_ParamAnts_datagrid_header_state");
            dgtxIsUseInv.Header = LangResouorce.GetText("ParamSettings_ParamAnts_datagrid_header_enableinv");

            int i = 1;
            foreach (AntInfo ai in rpViewModel.ocListAnts)
            {
                ai.Name = LangResouorce.GetText("ParamSettings_ParamAnts_AntInfo_Name") + i++;
                var Connected = LangResouorce.GetText("ParamSettings_ParamAnts_AntInfo_Connected");

                //chenxh 2022/08/01 添加
                if (ai.IsUseInv)
                    ai.ConnState = LangResouorce.GetText("ParamSettings_ParamAnts_AntInfo_Connected");
                else
                    ai.ConnState = LangResouorce.GetText("ParamSettings_ParamAnts_AntInfo_Unconnected");

                //chenxh 2022/08/01 注释
                //if (ai.ConnState == "连接" || ai.ConnState == "Connected"||ai.ConnState=="Bağlı")
                //    ai.ConnState = LangResouorce.GetText("ParamSettings_ParamAnts_AntInfo_Connected");
                //else if (ai.ConnState == "未连接" || ai.ConnState == "Unconnected"||ai.ConnState== "Bağlantısız")
                //    ai.ConnState = LangResouorce.GetText("ParamSettings_ParamAnts_AntInfo_Unconnected");
            }
            if (rpViewModel.ocListAnts.Count > 0)
                rpViewModel.ocListAnts[rpViewModel.ocListAnts.Count - 1].Name = LangResouorce.GetText("ParamSettings_ParamAnts_AntInfo_SetAll");
        }

        private void UserControl_IsVisibleChanged(object sender, DependencyPropertyChangedEventArgs e)
        {
            Debug.WriteLine("1111111111111111111111111111 OldValue:" + e.OldValue.ToString()+ "   NewValue:" + e.NewValue);
            if ((bool)e.NewValue == true)
            {
                if (rpViewModel.IsConnect)
                {
                    if (!HasGotParams)
                    {
                        if (GetParams() != 0)
                            return;
                    }
                }
            }
        }
    }
}
