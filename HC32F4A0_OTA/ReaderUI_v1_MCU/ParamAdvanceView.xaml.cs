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
using ModuleTech.Gen2;
using ReaderManager.Models;

namespace ReaderManager
{
    /// <summary>
    /// ParamAdvanceView.xaml 的交互逻辑
    /// </summary>
    public partial class ParamAdvanceView : UserControl
    {
        public ParamAdvanceView()
        {
            InitializeComponent();           
            CtrlListSources.AddComboBox(cbbGen2WriteMode);
            CtrlListSources.AddComboBox(cbbRegion);
            rpViewModel = ReaderParamsViewModel.GetReaderParamsViewModel();
            rootWnd = ReaderParamsViewModel.GetMainWindow();
            rootWnd.AllViews["ParamAdvanceView"] = this;
            DataContext = rpViewModel;
        }

        void CreateHopTab(uint[] freqs)
        {
            locHopTab.Clear();
            wpHoptab.Children.Clear();
            int DicCnt = Application.Current.Resources.MergedDictionaries.Count;
            foreach (uint freq in freqs)
            {
                CheckBox cbFreq = new CheckBox();
                cbFreq.Style = (Style)Application.Current.Resources.MergedDictionaries[DicCnt - 2]["paramCbFreqStyle"];
                cbFreq.Content = freq.ToString();
                wpHoptab.Children.Add(cbFreq);
                locHopTab.Add(freq);
            }
            CheckBox selall = new CheckBox();
            selall.SetResourceReference(CheckBox.ContentProperty, "ParamSettings_ParamAdvance_selall");
            selall.Click += OnCheckBoxClick;
            selall.Style = (Style)Application.Current.Resources.MergedDictionaries[DicCnt - 2]["paramCbFreqStyle"];
            wpHoptab.Children.Add(selall);
        }

        int locGen2Target = -1;
        int locGen2QValue = -1;
        int locGen2WriteMode = -1;

        int locGen2Session = -1;
        int locProfile = -1;
        int locFreqHopMode = -1;
        string locAntMaxDwellTime = "";
        bool locUniByAnt = false;
        bool locUniByBankData = false;
        bool locRecordHighestRssi = false;

        int locRegion = -1;
        List<uint> locHopTab = new List<uint>();

        public int GetParams(bool isTip = false)
        {
            try
            {
                rpViewModel.Gen2WriteMode = (int)(WriteMode)rpViewModel.ModReader.ParamGet("Gen2WriteMode");
                locGen2WriteMode = rpViewModel.Gen2WriteMode;
                rpViewModel.UniByAnt = (bool)rpViewModel.ModReader.ParamGet("IsTagDataUniqueByAnt");
                locUniByAnt = rpViewModel.UniByAnt;
                rpViewModel.UniByBankData = (bool)rpViewModel.ModReader.ParamGet("IsTagDataUniqueByEmddata");
                locUniByBankData = rpViewModel.UniByBankData;
                rpViewModel.RecordHighestRssi = (bool)rpViewModel.ModReader.ParamGet("IsTagdataRecordHighestRssi");
                locRecordHighestRssi = rpViewModel.RecordHighestRssi;

                if (rpViewModel.EnableFreqHopMode)
                {
                    rpViewModel.FreqHopMode = (int)rpViewModel.ModReader.ParamGet("HopFrequencyMode");
                    locFreqHopMode = rpViewModel.FreqHopMode;
                }

                if (rpViewModel.EnableAntMaxDwellTime)
                {
                    rpViewModel.AntMaxDwellTime = ((int)rpViewModel.ModReader.ParamGet("HopAntTime")).ToString();
                    locAntMaxDwellTime = rpViewModel.AntMaxDwellTime;
                }

                ModuleTech.Region rg = (ModuleTech.Region)rpViewModel.ModReader.ParamGet("Region");
                switch (rg)
                {
                    case ModuleTech.Region.CN:
                        rpViewModel.Region = 6;
                        break;
                    case ModuleTech.Region.EU:
                    case ModuleTech.Region.EU2:
                    case ModuleTech.Region.EU3:
                        rpViewModel.Region = 4;
                        break;
                    case ModuleTech.Region.IN:
                        rpViewModel.Region = 5;
                        break;
                    case ModuleTech.Region.JP:
                        rpViewModel.Region = 2;
                        break;
                    case ModuleTech.Region.KR:
                        rpViewModel.Region = 3;
                        break;
                    case ModuleTech.Region.NA:
                        rpViewModel.Region = 1;
                        break;
                    case ModuleTech.Region.PRC:
                        rpViewModel.Region = 0;
                        break;
                    case ModuleTech.Region.OPEN:
                        rpViewModel.Region = 7;
                        break;
                    case ModuleTech.Region.PRC2:
                        rpViewModel.Region = 8;
                        break;
                    default:
                        rpViewModel.Region = -1;
                        break;
                }
                locRegion = rpViewModel.Region;
                uint[] freqs = (uint[])rpViewModel.ModReader.ParamGet("FrequencyHopTable");
                CreateHopTab(freqs);
            }
            catch (Exception ex)
            {
                ReaderParamsViewModel.GetMainWindow().TipApiFailed(LangResouorce.GetText("Msg_Params_GetFailed"), ex);
                return -1;
            }
            if (isTip)
                ReaderParamsViewModel.GetMainWindow().TipSuccess(LangResouorce.GetText("Msg_Params_GetSuccess"));
            return 0;
        }

        public int SetParams()
        {
            if (rpViewModel.Gen2WriteMode == -1)
            {
                rootWnd.TipMessage(LangResouorce.GetText("Msg_PlsSelect") + LangResouorce.GetText("Msg_Space") +
                    LangResouorce.GetText("ParamSettings_ParamAdvance_gen2writemode"));
                return -1;
            }

            if (rpViewModel.Region == -1)
            {
                rootWnd.TipMessage(LangResouorce.GetText("Msg_PlsSelect") + LangResouorce.GetText("Msg_Space") +
                    LangResouorce.GetText("ParamSettings_ParamAdvance_region"));
                return -1;
            }
            if (rpViewModel.EnableFreqHopMode)
            {
                if (locFreqHopMode != rpViewModel.FreqHopMode)
                {
                    rpViewModel.ModReader.ParamSet("HopFrequencyMode", rpViewModel.FreqHopMode);
                    locFreqHopMode = rpViewModel.FreqHopMode;
                }
            }

            if (rpViewModel.EnableAntMaxDwellTime)
            {
                if (locAntMaxDwellTime != rpViewModel.AntMaxDwellTime)
                {
                    rpViewModel.ModReader.ParamSet("HopAntTime", int.Parse(rpViewModel.AntMaxDwellTime));
                    locAntMaxDwellTime = rpViewModel.AntMaxDwellTime;
                }
            }
            List<uint> hopfreqs = new List<uint>();
            if (locRegion == rpViewModel.Region)
            {                
                for (int i = 0; i < wpHoptab.Children.Count - 1; ++i)
                {
                    if ((bool)((CheckBox)wpHoptab.Children[i]).IsChecked)
                    {
                        hopfreqs.Add(uint.Parse((string)((CheckBox)wpHoptab.Children[i]).Content));
                    }
                }
                if (hopfreqs.Count == 0)
                {
                    rootWnd.TipMessage(LangResouorce.GetText("Msg_PlsSelect") + LangResouorce.GetText("Msg_Space") +
                        LangResouorce.GetText("ParamSettings_ParamAdvance_hoptab"));
                    return -1;
                }
            }

            try
            {
                
                if (locGen2WriteMode != rpViewModel.Gen2WriteMode)
                {
                    rpViewModel.ModReader.ParamSet("Gen2WriteMode", (WriteMode)rpViewModel.Gen2WriteMode);
                    locGen2WriteMode = rpViewModel.Gen2WriteMode;
                }
                if (locUniByAnt != rpViewModel.UniByAnt)
                {
                    rpViewModel.ModReader.ParamSet("IsTagDataUniqueByAnt", rpViewModel.UniByAnt);
                    locUniByAnt = rpViewModel.UniByAnt;
                }
                if (locUniByBankData != rpViewModel.UniByBankData)
                {
                    rpViewModel.ModReader.ParamSet("IsTagDataUniqueByEmddata", rpViewModel.UniByBankData);
                    locUniByBankData = rpViewModel.UniByBankData;
                }
                if (locRecordHighestRssi != rpViewModel.RecordHighestRssi)
                {
                    rpViewModel.ModReader.ParamSet("IsTagdataRecordHighestRssi", rpViewModel.RecordHighestRssi);
                    locRecordHighestRssi = rpViewModel.RecordHighestRssi;
                }

                bool loadhoptab = false;
                bool issetnewrg = false;
                if (locRegion != rpViewModel.Region)
                {
                    ModuleTech.Region rg = ModuleTech.Region.UNSPEC;
                    switch (rpViewModel.Region)
                    {
                        case 0:
                            rg = ModuleTech.Region.PRC;
                            break;
                        case 1:
                            rg = ModuleTech.Region.NA;
                            break;
                        case 2:
                            rg = ModuleTech.Region.JP;
                            break;
                        case 3:
                            rg = ModuleTech.Region.KR;
                            break;
                        case 4:
                            rg = ModuleTech.Region.EU3;
                            break;
                        case 5:
                            rg = ModuleTech.Region.IN;
                            break;
                        case 6:
                            rg = ModuleTech.Region.CN;
                            break;
                        case 7:
                            rg = ModuleTech.Region.OPEN;
                            break;
                        case 8:
                            rg = ModuleTech.Region.PRC2;
                            break;
                    }
                    rpViewModel.ModReader.ParamSet("Region", rg);
                    locRegion = rpViewModel.Region;
                    loadhoptab = true;
                    issetnewrg = true;
                }

                if (!issetnewrg)
                {
                    bool selnewtab = false;
                    if (hopfreqs.Count == locHopTab.Count)
                    {
                        for (int n = 0; n < hopfreqs.Count; ++n)
                        {
                            if (hopfreqs[n] != hopfreqs[n])
                            {
                                selnewtab = true;
                                break;
                            }
                        }
                    }
                    else
                        selnewtab = true;

                    if (selnewtab)
                    {
                        rpViewModel.ModReader.ParamSet("FrequencyHopTable", hopfreqs.ToArray());
                        CreateHopTab(hopfreqs.ToArray());
                    }
                }

                if (loadhoptab)
                {
                    uint[] freqs = (uint[])rpViewModel.ModReader.ParamGet("FrequencyHopTable");
                    CreateHopTab(freqs);
                }
            }
            catch (Exception ex)
            {
                rootWnd.TipApiFailed(LangResouorce.GetText("Msg_Params_SetFailed"), ex);
                return -1;
            }

            rootWnd.TipSuccess(LangResouorce.GetText("Msg_Params_SetSuccess"));
            return 0;
        }

        public int SetPerpetualParams()
        {
            List<uint> hopfreqs = new List<uint>();
            if (locRegion == rpViewModel.Region)
            {
                for (int i = 0; i < wpHoptab.Children.Count - 1; ++i)
                {
                    if ((bool)((CheckBox)wpHoptab.Children[i]).IsChecked)
                    {
                        hopfreqs.Add(uint.Parse((string)((CheckBox)wpHoptab.Children[i]).Content));
                    }
                }
                if (hopfreqs.Count == 0)
                {
                    rootWnd.TipMessage(LangResouorce.GetText("Msg_PlsSelect") + LangResouorce.GetText("Msg_Space") +
                        LangResouorce.GetText("ParamSettings_ParamAdvance_hoptab"));
                    return -1;
                }
            }

            try
            {
                ModuleTech.Region rg = ModuleTech.Region.UNSPEC;
                switch (rpViewModel.Region)
                {
                    case 0:
                        rg = ModuleTech.Region.PRC;
                        break;
                    case 1:
                        rg = ModuleTech.Region.NA;
                        break;
                    case 2:
                        rg = ModuleTech.Region.JP;
                        break;
                    case 3:
                        rg = ModuleTech.Region.KR;
                        break;
                    case 4:
                        rg = ModuleTech.Region.EU3;
                        break;
                    case 5:
                        rg = ModuleTech.Region.IN;
                        break;
                    case 6:
                        rg = ModuleTech.Region.CN;
                        break;
                    case 7:
                        rg = ModuleTech.Region.OPEN;
                        break;
                    case 8:
                        rg = ModuleTech.Region.PRC2;
                        break;
                }
                rpViewModel.ModReader.ParamSet("ModuleSave_Region", rg);
                locRegion = rpViewModel.Region;
                bool selnewtab = false;
               
                for (int n = 0; n < hopfreqs.Count; ++n)
                {
                    if (hopfreqs[n] != hopfreqs[n])
                    {
                        selnewtab = true;
                        break;
                    }
                }

                if (selnewtab)
                {
                    rpViewModel.ModReader.ParamSet("ModuleSave_Frenqency", hopfreqs.ToArray());
                    CreateHopTab(hopfreqs.ToArray());
                }

                //if (loadhoptab)
                //{
                //    uint[] freqs = (uint[])rpViewModel.ModReader.ParamGet("FrequencyHopTable");
                //    CreateHopTab(freqs);
                //}
            }
            catch (Exception ex)
            {
                rootWnd.TipApiFailed(LangResouorce.GetText("Msg_Params_SetFailed"), ex);
                return -1;
            }

            rootWnd.TipSuccess(LangResouorce.GetText("Msg_Params_SetSuccess"));
            return 0;
        }

        ReaderParamsViewModel rpViewModel = null;
        private void OnCheckBoxClick(object sender, RoutedEventArgs e)
        {
            CheckBox cb = (CheckBox)e.Source;
            for (int i = 0; i < wpHoptab.Children.Count; ++i)
                ((CheckBox)wpHoptab.Children[i]).IsChecked = cb.IsChecked;
        }
        private void UserControl_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            //Debug.WriteLine("this.Width:" + rootWnd.ActualWidth + " userCotlSize:" + mainWndSize);
            foreach (RowDefinition rf in gdMain.RowDefinitions)
            {
                if ((string)rf.Tag != "NoChange")
                    rf.Height = new GridLength(rf.MinHeight * (rootWnd.ActualHeight / rootWnd.mainWndHeightSize));
            }
            wpHoptab.Width = wpHoptab.MinWidth * (rootWnd.ActualWidth / rootWnd.mainWndSize);
        }

        MainWindow rootWnd;

        private void UserControl_Loaded(object sender, RoutedEventArgs e)
        {
            foreach (RowDefinition rf in gdMain.RowDefinitions)
            {
                if ((string)rf.Tag != "NoChange")
                    rf.Height = new GridLength(rf.MinHeight * (rootWnd.ActualHeight / rootWnd.mainWndHeightSize));
            }
            wpHoptab.Width = wpHoptab.MinWidth * (rootWnd.ActualWidth / rootWnd.mainWndSize);
        }

        bool HasGotParams = false;
        public void ResetView()
        {
            HasGotParams = false;
        }

        private void UserControl_IsVisibleChanged(object sender, DependencyPropertyChangedEventArgs e)
        {
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
