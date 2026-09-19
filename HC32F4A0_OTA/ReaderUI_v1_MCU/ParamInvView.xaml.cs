using ModuleTech;
using ReaderManager.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows;
using System.Windows.Controls;

namespace ReaderManager
{
    /// <summary>
    /// ParamInvView.xaml 的交互逻辑
    /// </summary>
    public partial class ParamInvView : UserControl
    {
        public ParamInvView()
        {
            InitializeComponent();
            rootWnd = ReaderParamsViewModel.GetMainWindow();
            rootWnd.AllViews["ParamInvView"] = this;
            rpViewModel = ReaderParamsViewModel.GetReaderParamsViewModel();
            DataContext = rpViewModel;
            rpViewModel.getQuickModeType = 0;

            
            //Gen2SessionBox.SelectedIndex = 0;
            //ProfileBox.SelectedIndex = 13;
            //rpViewModel.ModReader.ParamSet("Gen2Qvalue", -1);
            //rpViewModel.ModReader.ParamSet("Gen2Target", ModuleTech.Gen2.Target.A);
        }

        ReaderParamsViewModel rpViewModel = null;
        private void UserControl_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            foreach (RowDefinition rf in gdMain.RowDefinitions)
            {
                rf.Height = new GridLength(rf.MinHeight * (rootWnd.ActualHeight / rootWnd.mainWndHeightSize));
            }
        }

        MainWindow rootWnd;
        private void UserControl_Loaded(object sender, RoutedEventArgs e)
        {
            foreach (RowDefinition rf in gdMain.RowDefinitions)
            {
                rf.Height = new GridLength(rf.MinHeight * (rootWnd.ActualHeight / rootWnd.mainWndHeightSize));
            }
            int _getQuickModeType = rpViewModel.getQuickModeType;

            //没有连接之前默认减少Ex10快速模式
            var quickModeType = ((string[])Application.Current.Resources["quickModeType"]).ToList();
            quickModeType.RemoveAt(quickModeType.Count - 1);
            ModeType.ItemsSource = quickModeType.ToArray();

            if (rpViewModel.ModReader!=null&&(
                rpViewModel.ModReader.HwDetails.module == Reader.Module_Type.MODOULE_SLR5100 ||
                rpViewModel.ModReader.HwDetails.module == Reader.Module_Type.MODOULE_SLR5200 ||
                rpViewModel.ModReader.HwDetails.module == Reader.Module_Type.MODOULE_SLR5300)) {
                rpViewModel.IsFastInvMode = false;
                tsFastMode.IsEnabled = false;
            }

            if (rpViewModel.ModReader == null|| rpViewModel.HardwareVer=="未知"|| (int)rpViewModel.ModReader.HwDetails.module < 24)
                return;

            //连接之后且区域符合 开启Ex10快速模式
            var authentication = rpViewModel.HardwareVer.Split('.')[2];
            if (quickModeType != null && quickModeType.Count > 0)
            {
                if (CertificationCountry(authentication))
                {
                    quickModeType = ((string[])Application.Current.Resources["quickModeType"]).ToList();
                    ModeType.ItemsSource = quickModeType.ToArray();
                }
            }

            rpViewModel.getQuickModeType = _getQuickModeType;
            ModeType.SelectedIndex = _getQuickModeType;
        }
        int locGen2Target = -1;
        int locGen2QValue = -1;
        public int GetParams(bool isTip = false)
        {
            object obj = rpViewModel.ModReader.ParamGet("EmbededCmdOfInventory");
            if (obj == null)
            {
                rpViewModel.invEmdData.IsEnable = false;
                rpViewModel.invEmdData2.IsEnable = false;
                rpViewModel.invEmdData3.IsEnable = false;
            }
            else
            {
                EmbededCmdData[] ecd = (EmbededCmdData[])obj;
                if (ecd.Length >= 1) {
                    rpViewModel.invEmdData.StartAddr = ecd[0].StartAddr.ToString();
                    rpViewModel.invEmdData.Bank = (int)ecd[0].Bank;
                    rpViewModel.invEmdData.BlkCnt = (ecd[0].ByteCnt / 2).ToString();
                    uint pwd = (uint)rpViewModel.ModReader.ParamGet("AccessPassword");
                    if (pwd == 0)
                    {
                        rpViewModel.invEmdData.AcsPwd = "";
                    }
                    else
                    {
                        rpViewModel.invEmdData.AcsPwd = pwd.ToString("X8");
                    }
                }
                if (ecd.Length >= 2)
                {
                    rpViewModel.invEmdData2.StartAddr = ecd[1].StartAddr.ToString();
                    rpViewModel.invEmdData2.Bank = (int)ecd[1].Bank;
                    rpViewModel.invEmdData2.BlkCnt = (ecd[1].ByteCnt / 2).ToString();
                    uint pwd = (uint)rpViewModel.ModReader.ParamGet("AccessPassword");
                    if (pwd == 0)
                    {
                        rpViewModel.invEmdData2.AcsPwd = "";
                    }
                    else
                    {
                        rpViewModel.invEmdData2.AcsPwd = pwd.ToString("X8");
                    }
                }

                if (ecd.Length >= 3)
                {
                    rpViewModel.invEmdData3.StartAddr = ecd[2].StartAddr.ToString();
                    rpViewModel.invEmdData3.Bank = (int)ecd[2].Bank;
                    rpViewModel.invEmdData3.BlkCnt = (ecd[2].ByteCnt / 2).ToString();
                    uint pwd = (uint)rpViewModel.ModReader.ParamGet("AccessPassword");
                    if (pwd == 0)
                    {
                        rpViewModel.invEmdData3.AcsPwd = "";
                    }
                    else
                    {
                        rpViewModel.invEmdData3.AcsPwd = pwd.ToString("X8");
                    }
                }

            }
            try
            {
                rpViewModel.Gen2Session = (int)((ModuleTech.Gen2.Session)rpViewModel.ModReader.ParamGet("Gen2Session"));
                locGen2Session = rpViewModel.Gen2Session;

                int enc = (int)rpViewModel.ModReader.ParamGet("gen2tagEncoding");
                if (enc <= 3)
                    rpViewModel.Profile = enc;
                else if (enc == 45)
                    rpViewModel.Profile = 20;
                else if (enc > 100)
                {
                    if (enc == 101)
                        rpViewModel.Profile = 10;
                    if (enc == 103)
                        rpViewModel.Profile = 11;
                    if (enc == 105)
                        rpViewModel.Profile = 12;
                    if (enc == 107)
                        rpViewModel.Profile = 13;
                    if (enc == 111)
                        rpViewModel.Profile = 14;
                    if (enc == 112)
                        rpViewModel.Profile = 15;
                    if (enc == 113)
                        rpViewModel.Profile = 16;
                    if (enc == 115)
                        rpViewModel.Profile = 17;
                    if (enc == 203)
                        rpViewModel.Profile = 18;
                    if (enc == 220)
                        rpViewModel.Profile = 19;
                }
                else
                {
                    rpViewModel.Profile = enc - 12;
                }
                locProfile = rpViewModel.Profile;

                rpViewModel.Gen2Target = (int)(ModuleTech.Gen2.Target)rpViewModel.ModReader.ParamGet("Gen2Target");
                locGen2Target = rpViewModel.Gen2Target;
                rpViewModel.Gen2QValue = (int)rpViewModel.ModReader.ParamGet("Gen2Qvalue") + 1;
                locGen2QValue = rpViewModel.Gen2QValue;

                ModeType.SelectedIndex = rpViewModel.getQuickModeType;

            }
            catch (Exception ex)
            {
                ReaderParamsViewModel.GetMainWindow().TipApiFailed(LangResouorce.GetText("Msg_Params_GetFailed"), ex);
                return -1;
            }
            if (isTip)
            {
                ReaderParamsViewModel.GetMainWindow().TipSuccess(LangResouorce.GetText("Msg_Params_GetSuccess"));
            }
            return 0;
        }

        public int SetParams()
        {
            string tip = "";
            if (rpViewModel.invEmdData.IsEnable)
            {
                tip = rpViewModel.invEmdData.validParams();
                if (tip != "ok")
                {
                    rootWnd.TipMessage(tip);
                    return -1;
                }
            }
            if (!rpViewModel.IsFastInvMode)
            {
                if (rpViewModel.ReadDur.Trim() == string.Empty)
                {
                    rootWnd.TipMessage(LangResouorce.GetText("Msg_PlsInput") + LangResouorce.GetText("Msg_Space") +
                        LangResouorce.GetText("ParamSettings_ParamInv_readdur"));
                    return -1;
                }
                else
                {
                    int readdur = 0;
                    if (int.TryParse(rpViewModel.ReadDur.Trim(), out readdur))
                    {
                        if (readdur <= 0)
                        {
                            rootWnd.TipMessage(LangResouorce.GetText("ParamSettings_ParamInv_readdur") +
                                LangResouorce.GetText("Msg_Validation_is") +
                                LangResouorce.GetText("Msg_Validation_InvalidVal"));
                            return -1;
                        }
                    }
                    else
                    {
                        rootWnd.TipMessage(LangResouorce.GetText("ParamSettings_ParamInv_readdur") +
                            LangResouorce.GetText("Msg_Validation_is") +
                            LangResouorce.GetText("Msg_Validation_InvalidVal"));
                        return -1;
                    }
                }

                if (rpViewModel.SleepDur.Trim() == string.Empty)
                {
                    rootWnd.TipMessage(LangResouorce.GetText("Msg_PlsInput") + LangResouorce.GetText("Msg_Space") +
                        LangResouorce.GetText("ParamSettings_ParamInv_sleepdur"));
                    return -1;
                }
                else
                {
                    int sleepdur = 0;
                    if (int.TryParse(rpViewModel.SleepDur.Trim(), out sleepdur))
                    {
                        if (sleepdur < 0)
                        {
                            rootWnd.TipMessage(LangResouorce.GetText("ParamSettings_ParamInv_sleepdur") +
                                LangResouorce.GetText("Msg_Validation_is") +
                                LangResouorce.GetText("Msg_Validation_InvalidVal"));
                            return -1;
                        }
                    }
                    else
                    {
                        rootWnd.TipMessage(LangResouorce.GetText("ParamSettings_ParamInv_sleepdur") +
                            LangResouorce.GetText("Msg_Validation_is") +
                            LangResouorce.GetText("Msg_Validation_InvalidVal"));
                        return -1;
                    }
                }
            }
            if (locGen2Target != rpViewModel.Gen2Target)
            {
                rpViewModel.ModReader.ParamSet("Gen2Target", (ModuleTech.Gen2.Target)rpViewModel.Gen2Target);
                locGen2Target = rpViewModel.Gen2Target;
            }
            if (locGen2QValue != rpViewModel.Gen2QValue)
            {
                rpViewModel.ModReader.ParamSet("Gen2Qvalue", rpViewModel.Gen2QValue - 1);
                locGen2QValue = rpViewModel.Gen2QValue;
            }
            if (rpViewModel.Gen2Session == -1)
            {
                rootWnd.TipMessage(LangResouorce.GetText("Msg_PlsSelect") + LangResouorce.GetText("Msg_Space") +
                    "Gen2Session");
                return -1;
            }
            if (rpViewModel.Profile == -1)
            {
                rootWnd.TipMessage(LangResouorce.GetText("Msg_PlsSelect") + LangResouorce.GetText("Msg_Space") +
                    "Profile");
                return -1;
            }
            try
            {
                List<EmbededCmdData> list = new List<EmbededCmdData>();
                if (rpViewModel.invEmdData.IsEnable)
                    list.Add(rpViewModel.invEmdData.embededCmdData);

                if (rpViewModel.invEmdData2.IsEnable)
                    list.Add(rpViewModel.invEmdData2.embededCmdData);

                if (rpViewModel.invEmdData3.IsEnable)
                    list.Add(rpViewModel.invEmdData3.embededCmdData);

                if(list.Count>0)
                    rpViewModel.ModReader.ParamSet("EmbededCmdOfInventory", list.ToArray());
                else
                    rpViewModel.ModReader.ParamSet("EmbededCmdOfInventory",null);

                if (rpViewModel.invEmdData.AcsPwd != string.Empty)
                {
                    uint passwd = uint.Parse(rpViewModel.invEmdData.AcsPwd.Trim(), System.Globalization.NumberStyles.AllowHexSpecifier);
                    rpViewModel.ModReader.ParamSet("AccessPassword", passwd);
                }
                else
                {
                    rpViewModel.ModReader.ParamSet("AccessPassword", (uint)0);
                }

                if (locGen2Session != rpViewModel.Gen2Session)
                {
                    rpViewModel.ModReader.ParamSet("Gen2Session", (ModuleTech.Gen2.Session)rpViewModel.Gen2Session);
                    locGen2Session = rpViewModel.Gen2Session;
                }

                if (locProfile != rpViewModel.Profile)
                {
                    int enc = 0;
                    if (rpViewModel.Profile <= 3)
                    {
                        enc = rpViewModel.Profile;
                    }
                    else if (rpViewModel.Profile > 3&& rpViewModel.Profile<=9)
                    {
                        enc = rpViewModel.Profile + 12;
                    }
                    else {
                        enc = getGen2val(rpViewModel.Profile);
                    }

                    rpViewModel.ModReader.ParamSet("gen2tagEncoding", enc);
                    locProfile = rpViewModel.Profile;
                    
                }
            }
            catch (Exception ex)
            {
                rootWnd.TipApiFailed(LangResouorce.GetText("Msg_Params_SetFailed"), ex);
                return -1;
            }

            rpViewModel.getQuickModeType = ModeType.SelectedIndex;
            rootWnd.TipSuccess(LangResouorce.GetText("Msg_Params_SetSuccess"));
            return 0;
        }

        public int SetPerpetualParams() {

            if (rpViewModel.Gen2Session == -1)
            {
                rootWnd.TipMessage(LangResouorce.GetText("Msg_PlsSelect") + LangResouorce.GetText("Msg_Space") +
                    "Gen2Session");
                return -1;
            }
            if (rpViewModel.Profile == -1)
            {
                rootWnd.TipMessage(LangResouorce.GetText("Msg_PlsSelect") + LangResouorce.GetText("Msg_Space") +
                    "Profile");
                return -1;
            }
           
            rpViewModel.ModReader.ParamSet("ModuleSave_ProtocolConfig_Gen2_Session", (ModuleTech.Gen2.Session)rpViewModel.Gen2Session);
            locGen2Session = rpViewModel.Gen2Session;
           
            if (rpViewModel.Profile <= 3)
                rpViewModel.ModReader.ParamSet("ModuleSave_ProtocolConfig_Gen2_M", rpViewModel.Profile);
            else if (rpViewModel.Profile > 3 && rpViewModel.Profile <= 9)
                rpViewModel.ModReader.ParamSet("ModuleSave_ProtocolConfig_Gen2_M", 0x10 + rpViewModel.Profile - 4);
            else
            {
                rpViewModel.ModReader.ParamSet("ModuleSave_ProtocolConfig_Gen2_M", getGen2val(rpViewModel.Profile));
            }

            locProfile = rpViewModel.Profile;
            rpViewModel.ModReader.ParamSet("ModuleSave_ProtocolConfig_Gen2_Q", rpViewModel.Gen2QValue - 1);
            rpViewModel.ModReader.ParamSet("ModuleSave_ProtocolConfig_Gen2_Target", (ModuleTech.Gen2.Target)rpViewModel.Gen2Target);

            rootWnd.TipSuccess(LangResouorce.GetText("Msg_Params_SetSuccess"));
            return 0;
        }


        int locGen2Session = -1;
        int locProfile = -1;

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
                        {
                            return;
                        }
                    }
                }
            }
        }

        public int getGen2val(int name)
        {
            switch (name)
            {
                case 0:
                    return 0;
                case 1:
                    return 1;
                case 2:
                    return 2;
                case 3:
                    return 3;
                case 4:
                    return 16;
                case 5:
                    return 17;
                case 6:
                    return 18;
                case 7:
                    return 19;
                case 8:
                    return 20;
                case 9:
                    return 21;
                case 10:
                    return 101;
                case 11:
                    return 103;
                case 12:
                    return 105;
                case 13:
                    return 107;
                case 14:
                    return 111;
                case 15:
                    return 112;
                case 16:
                    return 113;
                case 17:
                    return 115;
                case 18:
                    return 203;
                case 19:
                    return 220;
                case 20:
                    return 45;
            }
            return 0;
        }

        private void ComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
        //    if ((int)rdr.HwDetails.module >= 24)
        //    {
        //        this.cbbgen2encode.Items.AddRange(new object[] {
        //        "FM0","M2","M4","M8","RF_MODE_1","RF_MODE_3","RF_MODE_5","RF_MODE_7",
        //        "RF_MODE_11","RF_MODE_12","RF_MODE_13","RF_MODE_15","RF_MODE_103","RF_MODE_120","RF_MODE_345"});
        //    }
        //    else
        //    {
        //        this.cbbgen2encode.Items.AddRange(new object[] {
        //        "FM0","M2","M4","M8","PROFILE 0","PROFILE 1","PROFILE 2","PROFILE 3","PROFILE 4","PROFILE 5",});
        //    }

            if (rpViewModel.ModReader == null)
                return;

            if (rpViewModel.getQuickModeType == 0)
                rpViewModel.IsQuickMode = true;
            else if (rpViewModel.getQuickModeType == 1)
                rpViewModel.IsQuickMode = false;
            else if (rpViewModel.getQuickModeType == 2)
                rpViewModel.IsQuickMode = false;

            if (rpViewModel.getQuickModeType == 0)
            {
                rpViewModel.IsQuickMode = true;
                Gen2SessionBox.SelectedIndex = 0;
                ProfileBox.SelectedIndex = 13;
                rpViewModel.ModReader.ParamSet("Gen2Qvalue", -1);
                rpViewModel.ModReader.ParamSet("Gen2Target", ModuleTech.Gen2.Target.A);
            }
            else if (rpViewModel.getQuickModeType == 1)
            {
                rpViewModel.IsQuickMode = false;
                rpViewModel.Gen2Session = 0;

                if ((int)rpViewModel.ModReader.HwDetails.module >= 24)
                    rpViewModel.Profile = 16;
                else
                    rpViewModel.Profile = 5;

                rpViewModel.Gen2Target = 2;
                rpViewModel.Gen2QValue = 0;
            }
            else if (rpViewModel.getQuickModeType == 2)
            {
                rpViewModel.IsQuickMode = false;
                rpViewModel.Gen2Session = 0;

                if ((int)rpViewModel.ModReader.HwDetails.module >= 24)
                    rpViewModel.Profile = 14;
                else
                    rpViewModel.Profile = 7;

                rpViewModel.Gen2Target = 2;
                rpViewModel.Gen2QValue = 3;
            }
            else if (rpViewModel.getQuickModeType == 3) {
                rpViewModel.IsQuickMode = false;
            }

            Gen2SessionBox.IsEnabled = rpViewModel.IsQuickMode;
            ProfileBox.IsEnabled = rpViewModel.IsQuickMode;
            TargetBox.IsEnabled = rpViewModel.IsQuickMode;
            gen2QstrsBox.IsEnabled = rpViewModel.IsQuickMode;
        }

        private void ComboBox_SelectionChanged_1(object sender, SelectionChangedEventArgs e)
        {
            //if (rpViewModel.invEmdData.Bank == 1)
            //    StartAddrBox.Text = "2";
            //else
            //    StartAddrBox.Text = "0";

            //rpViewModel.invEmdData.BlkCnt = "4";
            //rpViewModel.invEmdData.StartAddr = StartAddrBox.Text;
            //BlkCntBox.Text = "4";
        }
        private bool CertificationCountry(string value)
        {
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
