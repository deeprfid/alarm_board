using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using ModuleTech.Gen2;
using ModuleTech;
using System.ComponentModel;
using System.Collections.ObjectModel;
using System.Windows;

namespace ReaderManager.Models
{
    public class InvEmbeddedData: INotifyPropertyChanged
    {
        public InvEmbeddedData()
        {
            Reset();
        }
        public void Reset()
        {
            Bank_ = -1;
            StartAddr_ = "";
            BlkCnt_ = "";
            AcsPwd_ = "";
            IsEnable_ = false;
        }
        public EmbededCmdData embededCmdData
        {
            get
            {
                if (IsEnable_)
                    return new EmbededCmdData((MemBank)Bank_, uint.Parse(StartAddr_),
                        (byte)(int.Parse(BlkCnt_) * 2));
                else
                    return null;
            }
        }
        public string validParams()
        {
            if (Bank_ == -1)
                return LangResouorce.GetText("Msg_PlsSelect") + " Bank";


            if (StartAddr_.Trim() == string.Empty)
                return LangResouorce.GetText("Msg_PlsInput") + LangResouorce.GetText("Msg_Space") +
                    LangResouorce.GetText("ParamSettings_ParamInv_adddata_combobox_startaddr_hint");
            else
            {
                int staddr = 0;
                if (int.TryParse(StartAddr_.Trim(), out staddr))
                {
                    if (staddr < 0)
                        return LangResouorce.GetText("ParamSettings_ParamInv_adddata_combobox_startaddr_hint") +
                            LangResouorce.GetText("Msg_Validation_is") +
                            LangResouorce.GetText("Msg_Validation_InvalidVal");
                }
                else
                    return LangResouorce.GetText("ParamSettings_ParamInv_adddata_combobox_startaddr_hint") +
                        LangResouorce.GetText("Msg_Validation_is") +
                        LangResouorce.GetText("Msg_Validation_InvalidVal");

            }

            if (BlkCnt_.Trim() == string.Empty)
                return LangResouorce.GetText("Msg_PlsInput") + LangResouorce.GetText("Msg_Space") +
                    LangResouorce.GetText("ParamSettings_ParamInv_adddata_combobox_blocks_hint");
            else
            {
                int blkcnt = 0;
                if (int.TryParse(BlkCnt_.Trim(), out blkcnt))
                {
                    if (blkcnt <= 0)
                        return LangResouorce.GetText("ParamSettings_ParamInv_adddata_combobox_blocks_hint") +
                            LangResouorce.GetText("Msg_Validation_is") +
                            LangResouorce.GetText("Msg_Validation_InvalidVal");
                }
                else
                    return LangResouorce.GetText("ParamSettings_ParamInv_adddata_combobox_blocks_hint") +
                        LangResouorce.GetText("Msg_Validation_is") +
                        LangResouorce.GetText("Msg_Validation_InvalidVal");

            }

            if (AcsPwd_.Trim() != string.Empty)
            {
                if (AcsPwd_.Trim().Length != 8)
                    return LangResouorce.GetText("ParamSettings_ParamInv_adddata_combobox_pwd") +
                                           LangResouorce.GetText("Msg_Validation_is") +
                                           LangResouorce.GetText("Msg_Validation_InvalidVal");
                else
                {
                    if (ValidatioAlgorithm.IsValidHexstr(AcsPwd_.Trim(), 1000) != 0)
                        return LangResouorce.GetText("ParamSettings_ParamInv_adddata_combobox_pwd") +
                              LangResouorce.GetText("Msg_Validation_is") +
                              LangResouorce.GetText("Msg_Validation_InvalidVal");
                }
            }       
            return "ok";
        }

        public event PropertyChangedEventHandler PropertyChanged;
        int Bank_;
        string StartAddr_;
        string BlkCnt_;
        string AcsPwd_;
        bool IsEnable_;
        public bool IsEnable
        {
            get { return IsEnable_; }
            set
            {
                if (value != IsEnable_)
                {
                    IsEnable_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("IsEnable"));
                }
            }
        }

        public int Bank
        {
            get { return Bank_; }
            set
            {
                if (value != Bank_)
                {
                    Bank_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("Bank"));
                }
            }
        }

        public string StartAddr
        {
            get { return StartAddr_; }
            set
            {
                if (value != StartAddr_)
                {
                    StartAddr_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("StartAddr"));
                }
            }
        }

        public string BlkCnt
        {
            get { return BlkCnt_; }
            set
            {
                if (value != BlkCnt_)
                {
                    BlkCnt_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("BlkCnt"));
                }
            }
        }

        public string AcsPwd
        {
            get { return AcsPwd_; }
            set
            {
                if (value != AcsPwd_)
                {
                    AcsPwd_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("AcsPwd"));
                }
            }
        }
    }

    public class TagFilterViewModel: INotifyPropertyChanged
    {
        public event PropertyChangedEventHandler PropertyChanged;
        int Bank_;
        int Match_;
        string StartAddr_;
        int MaskFormat_;
        string Mask_;
        bool IsUseInv_;
        bool IsUseTagOp_;

        public string validParams()
        {
            if (Bank_ == -1)
                return LangResouorce.GetText("Msg_PlsSelect") + " Bank";

            if (Match_ == -1)
                return LangResouorce.GetText("Msg_PlsSelect") + LangResouorce.GetText("Msg_Space") +
                    LangResouorce.GetText("FlyView_Bottom_TagFilter_FilterRule");

            if (StartAddr_.Trim() == string.Empty)
                return LangResouorce.GetText("Msg_PlsInput") + LangResouorce.GetText("Msg_Space") +
                    LangResouorce.GetText("ParamSettings_ParamInv_adddata_combobox_startaddr_hint");
            else
            {
                int staddr = 0;
                if (int.TryParse(StartAddr_.Trim(), out staddr))
                {
                    if (staddr < 0)
                        return LangResouorce.GetText("ParamSettings_ParamInv_adddata_combobox_startaddr_hint") +
                            LangResouorce.GetText("Msg_Validation_is") +
                            LangResouorce.GetText("Msg_Validation_InvalidVal");
                }
                else
                    return LangResouorce.GetText("ParamSettings_ParamInv_adddata_combobox_startaddr_hint") +
                        LangResouorce.GetText("Msg_Validation_is") +
                        LangResouorce.GetText("Msg_Validation_InvalidVal");

            }

            if (MaskFormat_ == -1)
                return LangResouorce.GetText("Msg_PlsSelect") + LangResouorce.GetText("Msg_Space") +
                    LangResouorce.GetText("FlyView_Bottom_TagFilter_MaskFormat");

            int res = 0;
            if (MaskFormat_ == 0)
                res = ValidatioAlgorithm.IsValidBinaryStr(Mask_.Trim());
            else if (MaskFormat_ == 1)
                res = ValidatioAlgorithm.IsValidHexstr(Mask_.Trim(), 1000);

            if (res == -3)
                return LangResouorce.GetText("FlyView_Bottom_TagFilter_FilterMask") +
                       LangResouorce.GetText("Msg_Validation_is") +
                       LangResouorce.GetText("Msg_Validation_InvalidEmpty");
            else if (res == -1)
                return LangResouorce.GetText("FlyView_Bottom_TagFilter_FilterMask") +
                    LangResouorce.GetText("Msg_Validation_is") +
                    LangResouorce.GetText("Msg_Validation_InvalidVal");

            return "ok";
        }
        public Gen2TagFilter GetTagFilter(int opty)
        {
            if (opty == 1)
            {
                if (!IsUseInv_)
                    return null;
            }
            if (opty == 2)
            {
                if (!IsUseTagOp_)
                    return null;
            }

            byte[] filterbytes = null;
            int bitlen = 0;
            if (MaskFormat == 0)
            {
                filterbytes = new byte[(Mask.Trim().Length - 1) / 8 + 1];
                for (int c = 0; c < filterbytes.Length; ++c)
                    filterbytes[c] = 0;

                int bitcnt = 0;
                foreach (Char ch in Mask.Trim())
                {
                    if (ch == '1')
                        filterbytes[bitcnt / 8] |= (byte)(0x01 << (7 - bitcnt % 8));
                    bitcnt++;
                }
                bitlen = Mask.Trim().Length;
            }
            else
            {
                string hexstr = Mask.Trim();
                if (hexstr.Length % 2 != 0)
                    hexstr += "0";
                filterbytes = ByteFormat.FromHex(hexstr);
                bitlen = Mask.Trim().Length * 4;
            }
            return new Gen2TagFilter(bitlen, filterbytes, (MemBank)Bank + 1,
                int.Parse(StartAddr), Match == 1);

        }
        public TagFilterViewModel()
        {
            Bank_ = -1;
            Match_ = -1;
            StartAddr_ = "";
            MaskFormat_ = -1;
            Mask_ = "";
            IsUseInv_ = false;
            IsUseTagOp_ = false;
        }
        public bool IsUseInv
        {
            get { return IsUseInv_; }
            set
            {
                if (value != IsUseInv_)
                {
                    IsUseInv_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("IsUseInv"));
                }
            }
        }

        public bool IsUseTagOp
        {
            get { return IsUseTagOp_; }
            set
            {
                if (value != IsUseTagOp_)
                {
                    IsUseTagOp_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("IsUseTagOp"));
                }
            }
        }

        public int Bank
        {
            get { return Bank_; }
            set
            {
                if (value != Bank_)
                {
                    Bank_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("Bank"));
                }
            }
        }

        public int Match
        {
            get { return Match_; }
            set
            {
                if (value != Match_)
                {
                    Match_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("Match"));
                }
            }
        }

        public string StartAddr
        {
            get { return StartAddr_; }
            set
            {
                if (value != StartAddr_)
                {
                    StartAddr_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("StartAddr"));
                }
            }
        }

        public string Mask
        {
            get { return Mask_; }
            set
            {
                if (value != Mask_)
                {
                    Mask_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("Mask"));
                }
            }
        }

        public int MaskFormat
        {
            get { return MaskFormat_; }
            set
            {
                if (value != MaskFormat_)
                {
                    MaskFormat_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("MaskFormat"));
                }
            }
        }
    }
    public class ReaderParamsViewModel: INotifyPropertyChanged
    {
        public ReaderParamsViewModel()
        {
            tfViewModel = new TagFilterViewModel();
            invEmdData = new InvEmbeddedData();
            invEmdData2 = new InvEmbeddedData();
            invEmdData3 = new InvEmbeddedData();
            ReadDur = "150";
            SleepDur = "0";
            UpFwProgress = 0;
            UpdFwPercent = "0%";
            IsQuickMode = true;
            IsUpdFw = false;
            ResetReaderParams();
        }
        public void SwitchLanguage()
        {
            //
            if (HardwareVer == "Unknown" || HardwareVer == "未知"|| HardwareVer == "Bilinmeyen")
            {
                HardwareVer = LangResouorce.GetText("Connect_label_unknown");
                SoftwareVer = LangResouorce.GetText("Connect_label_unknown");
                ModType = LangResouorce.GetText("Connect_label_unknown");
                BoardType = LangResouorce.GetText("Connect_label_unknown");
            }
        }
        public void ResetReaderParams()
        {
            IsConnect = false;
            IsInventory = false;
            IsReadTag = false;
            HardwareVer = LangResouorce.GetText("Connect_label_unknown");
            SoftwareVer = LangResouorce.GetText("Connect_label_unknown");
            ModType = LangResouorce.GetText("Connect_label_unknown");
            BoardType = LangResouorce.GetText("Connect_label_unknown");
            //ReaderAddr = "";
            IsFastInvMode = true;
            IsPotlPrint = false;
            PotlString = "";
            InventoryErrLog = "";


            Gen2Session = -1;
            FreqHopMode = -1;
            Profile = -1;
            AntMaxDwellTime = "";
            EnableFreqHopMode = true;
            EnableAntMaxDwellTime = true;

            Ip = "";
            SubnetMask = "";
            Gateway = "";
            EnableIp = true;

            Key = "";
            SSID = "";
            AuthMode = -1;
            KeyType = -1;
            EnableWireless = false;
            IsWirelessOn = false;

            Gen2Target = -1;
            Gen2QValue = -1;
            Gen2WriteMode = -1;
            UniByAnt = false;
            UniByBankData = false;
            RecordHighestRssi = false;
            Region = -1;

            TagOpType = -1;
            TagOpAnt = -1;
            TagOpBank = -1;
            TagOpLockObj = -1;
            TagOpLockType = -1;
            TagOpAccessPwd = "";
            TagOpKillPwd = "";
            TagOpStartAddr = "";
            TagOpBlkCnt = "";
            TagOpHexData = "";

            UpdFwFilePath = "";
            UpdFwReaderAddr = "";
            UpdFwVersion = "";          

            EnableGpi1 = false;
            EnableGpi2 = false;
            EnableGpi3 = false;
            EnableGpi4 = false;

            EnableGpo1 = false;
            EnableGpo2 = false;
            EnableGpo3 = false;
            EnableGpo4 = false;

            VswrCheckAnt = -1;
            VswrCheckPower = "";

            workTime = 0;
            listTagsReadCount=0;
            listTags = new ObservableCollection<TagInfo>();
            dicUniTags.Clear();

            ocListAnts.Clear();
            invEmdData.Reset();

            invEmdData2.Reset();
            invEmdData3.Reset();
        }
            
        public static ReaderParamsViewModel GetReaderParamsViewModel()
        {
            return GetMainWindow().rdpmViewModel;
        }
        public static MainWindow GetMainWindow()
        {
            return Application.Current.Windows.Cast<Window>().FirstOrDefault(window => window is MainWindow) as MainWindow;
        }

        public event PropertyChangedEventHandler PropertyChanged;

        public TagFilterViewModel tfViewModel { get; set; }
        public InvEmbeddedData invEmdData { get; set; }
        public InvEmbeddedData invEmdData2 { get; set; }
        public InvEmbeddedData invEmdData3 { get; set; }
        public Reader ModReader { get; set; }

        public int AntPortNumber { get; set; }
        public int MaxTxPower { get; set; }
        public int MinTxPower { get; set; }

        public bool IsGpi5 { get; set; }//盘点是否触发GPI5
        public int IsGpi5Time { get; set; }//触发GPI5持续时间

        public bool IsStartStop { get; set; }//是否开启自动停止
        public int GetStopTime { get; set; }//读取多少时间停止(ms)
        public int GetStopCount { get; set; }//读取多少数量停止

        public int GetForCount { get; set; }//盘点次数
        public int StopTime { get; set; }// 盘点开始时间
        public int workTime { get; set; }//盘点时长(ms)

        public string Versions { get; set; }// 软件版本

        public int getQuickModeType { get; set; } //快速模式类型

        public bool IsQuickMode { get; set; } //

        public string InventoryErrLog { get; set; }
        public int[] InvAnts { get; set; }

        public ObservableCollection<AntInfo> ocListAnts = new ObservableCollection<AntInfo>();
        public ObservableCollection<TagInfo> listTags = new ObservableCollection<TagInfo>();
        public Dictionary<string, int> dicUniTags = new Dictionary<string, int>();
        public int listTagsReadCount { get; set; }

        bool IsPotlPrint_;
        int GetTime_;
        int GetCount_;
        string PotlString_;
        bool IsInventory_;
        bool IsConnect_;
        bool IsReadTag_;
      
        string HardwareVer_;
        string SoftwareVer_;
        string MainboardVer_;
        string BoardType_;
        string ModType_;
        string ReaderAddr_;
        bool IsFastInvMode_;
        bool IsStartMode_;
        string ReadDur_;
        string SleepDur_;

        int Gen2Session_;
        int FreqHopMode_;
        int Profile_;
        string AntMaxDwellTime_;
        bool EnableFreqHopMode_;
        bool EnableAntMaxDwellTime_;

        string Ip_;
        string SubnetMask_;
        string Gateway_;
        bool EnableIp_;

        string Key_;
        string SSID_;
        int AuthMode_;
        int KeyType_;
        bool EnableWireless_;
        bool IsWirelessOn_;

        int Gen2Target_;
        int Gen2QValue_;
        int Gen2WriteMode_;
        int Region_;
        bool UniByAnt_;
        bool UniByBankData_;
        bool RecordHighestRssi_;

        int TagOpType_;
        int TagOpAnt_;
        int TagOpBank_;
        int TagOpLockObj_;
        int TagOpLockType_;
        string TagOpAccessPwd_;
        string TagOpKillPwd_;
        string TagOpStartAddr_;
        string TagOpBlkCnt_;
        string TagOpHexData_;

        bool EnableGpi1_;
        bool EnableGpi2_;
        bool EnableGpi3_;
        bool EnableGpi4_;

        bool EnableGpo1_;
        bool EnableGpo2_;
        bool EnableGpo3_;
        bool EnableGpo4_;

        string UpdFwFilePath_;
        string UpdFwReaderAddr_;
        string UpdFwVersion_;
        double UpFwProgress_;
        string UpdFwPercent_;
        bool IsUpdFw_;

        int VswrCheckAnt_;
        string VswrCheckPower_;
        public int VswrCheckAnt
        {
            get { return VswrCheckAnt_; }
            set
            {
                if (value != VswrCheckAnt_)
                {
                    VswrCheckAnt_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("VswrCheckAnt"));
                }
            }
        }
        public string VswrCheckPower
        {
            get { return VswrCheckPower_; }
            set
            {
                if (value != VswrCheckPower_)
                {
                    VswrCheckPower_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("VswrCheckPower"));
                }
            }
        }
        public string UpdFwPercent
        {
            get { return UpdFwPercent_; }
            set
            {
                if (value != UpdFwPercent_)
                {
                    UpdFwPercent_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("UpdFwPercent"));
                }
            }
        }
        public bool IsUpdFw
        {
            get { return IsUpdFw_; }
            set
            {
                if (value != IsUpdFw_)
                {
                    IsUpdFw_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("IsUpdFw"));
                }
            }
        }

        public bool Is5300or3500 { get; set; }

        public double UpFwProgress
        {
            get { return UpFwProgress_; }
            set
            {
                if (value != UpFwProgress_)
                {
                    UpFwProgress_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("UpFwProgress"));
                }
            }
        }
        public string UpdFwVersion
        {
            get { return UpdFwVersion_; }
            set
            {
                if (value != UpdFwVersion_)
                {
                    UpdFwVersion_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("UpdFwVersion"));
                }
            }
        }
        public string UpdFwReaderAddr
        {
            get { return UpdFwReaderAddr_; }
            set
            {
                if (value != UpdFwReaderAddr_)
                {
                    UpdFwReaderAddr_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("UpdFwReaderAddr"));
                }
            }
        }

        public string UpdFwFilePath
        {
            get { return UpdFwFilePath_; }
            set
            {
                if (value != UpdFwFilePath_)
                {
                    UpdFwFilePath_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("UpdFwFilePath"));
                }
            }
        }
        public bool EnableGpo1
        {
            get { return EnableGpo1_; }
            set
            {
                if (value != EnableGpo1_)
                {
                    EnableGpo1_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("EnableGpo1"));
                }
            }
        }
        public bool EnableGpo2
        {
            get { return EnableGpo2_; }
            set
            {
                if (value != EnableGpo2_)
                {
                    EnableGpo2_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("EnableGpo2"));
                }
            }
        }
        public bool EnableGpo3
        {
            get { return EnableGpo3_; }
            set
            {
                if (value != EnableGpo3_)
                {
                    EnableGpo3_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("EnableGpo3"));
                }
            }
        }
        public bool EnableGpo4
        {
            get { return EnableGpo4_; }
            set
            {
                if (value != EnableGpo4_)
                {
                    EnableGpo4_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("EnableGpo4"));
                }
            }
        }

        public bool EnableGpi1
        {
            get { return EnableGpi1_; }
            set
            {
                if (value != EnableGpi1_)
                {
                    EnableGpi1_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("EnableGpi1"));
                }
            }
        }
        public bool EnableGpi2
        {
            get { return EnableGpi2_; }
            set
            {
                if (value != EnableGpi2_)
                {
                    EnableGpi2_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("EnableGpi2"));
                }
            }
        }
        public bool EnableGpi3
        {
            get { return EnableGpi3_; }
            set
            {
                if (value != EnableGpi3_)
                {
                    EnableGpi3_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("EnableGpi3"));
                }
            }
        }
        public bool EnableGpi4
        {
            get { return EnableGpi4_; }
            set
            {
                if (value != EnableGpi4_)
                {
                    EnableGpi4_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("EnableGpi4"));
                }
            }
        }

        public string TagOpHexData
        {
            get { return TagOpHexData_; }
            set
            {
                if (value != TagOpHexData_)
                {
                    TagOpHexData_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("TagOpHexData"));
                }
            }
        }

        public int TagOpLockType
        {
            get { return TagOpLockType_; }
            set
            {
                if (value != TagOpLockType_)
                {
                    TagOpLockType_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("TagOpLockType"));
                }
            }
        }
        public int TagOpLockObj
        {
            get { return TagOpLockObj_; }
            set
            {
                if (value != TagOpLockObj_)
                {
                    TagOpLockObj_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("TagOpLockObj"));
                }
            }
        }
        public int TagOpBank
        {
            get { return TagOpBank_; }
            set
            {
                if (value != TagOpBank_)
                {
                    TagOpBank_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("TagOpBank"));
                }
            }
        }

        public string TagOpBlkCnt
        {
            get { return TagOpBlkCnt_; }
            set
            {
                if (value != TagOpBlkCnt_)
                {
                    TagOpBlkCnt_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("TagOpBlkCnt"));
                }
            }
        }
        public string TagOpStartAddr
        {
            get { return TagOpStartAddr_; }
            set
            {
                if (value != TagOpStartAddr_)
                {
                    TagOpStartAddr_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("TagOpStartAddr"));
                }
            }
        }
        public string TagOpKillPwd
        {
            get { return TagOpKillPwd_; }
            set
            {
                if (value != TagOpKillPwd_)
                {
                    TagOpKillPwd_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("TagOpKillPwd"));
                }
            }
        }
        public string TagOpAccessPwd
        {
            get { return TagOpAccessPwd_; }
            set
            {
                if (value != TagOpAccessPwd_)
                {
                    TagOpAccessPwd_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("TagOpAccessPwd"));
                }
            }
        }
        public int TagOpAnt
        {
            get { return TagOpAnt_; }
            set
            {
                if (value != TagOpAnt_)
                {
                    TagOpAnt_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("TagOpAnt"));
                }
            }
        }
        public int TagOpType
        {
            get { return TagOpType_; }
            set
            {
                if (value != TagOpType_)
                {
                    TagOpType_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("TagOpType"));
                }
            }
        }

        public int Region
        {
            get { return Region_; }
            set
            {
                if (value != Region_)
                {
                    Region_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("Region"));
                }
            }
        }

        public int Gen2Target
        {
            get { return Gen2Target_; }
            set
            {
                if (value != Gen2Target_)
                {
                    Gen2Target_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("Gen2Target"));
                }
            }
        }
        public int Gen2QValue
        {
            get { return Gen2QValue_; }
            set
            {
                if (value != Gen2QValue_)
                {
                    Gen2QValue_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("Gen2QValue"));
                }
            }
        }
        public int Gen2WriteMode
        {
            get { return Gen2WriteMode_; }
            set
            {
                if (value != Gen2WriteMode_)
                {
                    Gen2WriteMode_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("Gen2WriteMode"));
                }
            }
        }

        public bool UniByAnt
        {
            get { return UniByAnt_; }
            set
            {
                if (value != UniByAnt_)
                {
                    UniByAnt_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("UniByAnt"));
                }
            }
        }
        public bool UniByBankData
        {
            get { return UniByBankData_; }
            set
            {
                if (value != UniByBankData_)
                {
                    UniByBankData_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("UniByBankData"));
                }
            }
        }
        public bool RecordHighestRssi
        {
            get { return RecordHighestRssi_; }
            set
            {
                if (value != RecordHighestRssi_)
                {
                    RecordHighestRssi_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("RecordHighestRssi"));
                }
            }
        }
        public bool IsWirelessOn
        {
            get { return IsWirelessOn_; }
            set
            {
                if (value != IsWirelessOn_)
                {
                    IsWirelessOn_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("IsWirelessOn"));
                }
            }
        }
        public bool EnableWireless
        {
            get { return EnableWireless_; }
            set
            {
                if (value != EnableWireless_)
                {
                    EnableWireless_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("EnableWireless"));
                }
            }
        }
        public int KeyType
        {
            get { return KeyType_; }
            set
            {
                if (value != KeyType_)
                {
                    KeyType_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("KeyType"));
                }
            }
        }
        public int AuthMode
        {
            get { return AuthMode_; }
            set
            {
                if (value != AuthMode_)
                {
                    AuthMode_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("AuthMode"));
                }
            }
        }
        public string SSID
        {
            get { return SSID_; }
            set
            {
                if (value != SSID_)
                {
                    SSID_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("SSID"));
                }
            }
        }
        public string Key
        {
            get { return Key_; }
            set
            {
                if (value != Key_)
                {
                    Key_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("Key"));
                }
            }
        }

        public string Gateway
        {
            get { return Gateway_; }
            set
            {
                if (value != Gateway_)
                {
                    Gateway_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("Gateway"));
                }
            }
        }

        public string SubnetMask
        {
            get { return SubnetMask_; }
            set
            {
                if (value != SubnetMask_)
                {
                    SubnetMask_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("SubnetMask"));
                }
            }
        }

        public string Ip
        {
            get { return Ip_; }
            set
            {
                if (value != Ip_)
                {
                    Ip_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("Ip"));
                }
            }
        }

        public bool EnableIp
        {
            get { return EnableIp_; }
            set
            {
                if (value != EnableIp_)
                {
                    EnableIp_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("EnableIp"));
                }
            }
        }
        public bool EnableAntMaxDwellTime
        {
            get { return EnableAntMaxDwellTime_; }
            set
            {
                if (value != EnableAntMaxDwellTime_)
                {
                    EnableAntMaxDwellTime_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("EnableAntMaxDwellTime"));
                }
            }
        }

        public bool EnableFreqHopMode
        {
            get { return EnableFreqHopMode_; }
            set
            {
                if (value != EnableFreqHopMode_)
                {
                    EnableFreqHopMode_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("EnableFreqHopMode"));
                }
            }
        }
        public string AntMaxDwellTime
        {
            get { return AntMaxDwellTime_; }
            set
            {
                if (value != AntMaxDwellTime_)
                {
                    AntMaxDwellTime_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("AntMaxDwellTime"));
                }
            }
        }

        public int FreqHopMode
        {
            get { return FreqHopMode_; }
            set
            {
                if (value != FreqHopMode_)
                {
                    FreqHopMode_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("FreqHopMode"));
                }
            }
        }

        public int Gen2Session
        {
            get { return Gen2Session_; }
            set
            {
                if (value != Gen2Session_)
                {
                    Gen2Session_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("Gen2Session"));
                }
            }
        }

        public string PotlString
        {
            get { return PotlString_; }
            set
            {
                if (value != PotlString_)
                {
                    PotlString_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("PotlString"));
                }
            }
        }
        public bool IsPotlPrint
        {
            get { return IsPotlPrint_; }
            set
            {
                if (value != IsPotlPrint_)
                {
                    IsPotlPrint_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("IsPotlPrint"));
                }
            }
        }

        public int GetTime
        {
            get { return GetTime_; }
            set
            {
                if (value != GetTime_)
                {
                    GetTime_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("GetTime"));
                }
            }
        }

        public int GetCount
        {
            get { return GetCount_; }
            set
            {
                if (value != GetCount_)
                {
                    GetCount_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("GetCount"));
                }
            }
        }

        public string ReadDur
        {
            get { return ReadDur_; }
            set
            {
                if (value != ReadDur_)
                {
                    ReadDur_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("ReadDur"));
                }
            }
        }
        public string SleepDur
        {
            get { return SleepDur_; }
            set
            {
                if (value != SleepDur_)
                {
                    SleepDur_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("SleepDur"));
                }
            }
        }

        public bool IsFastInvMode
        {
            get { return IsFastInvMode_; }
            set
            {
                if (value != IsFastInvMode_)
                {
                    IsFastInvMode_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("IsFastInvMode"));
                }
            }
        }
        public bool IsStartMode
        {
            get { return IsStartMode_; }
            set
            {
                if (value != IsStartMode_)
                {
                    IsStartMode_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("IsStartMode"));
                }
            }
        }

        public string ReaderAddr
        {
            get { return ReaderAddr_; }
            set
            {
                if (value != ReaderAddr_)
                {
                    ReaderAddr_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("ReaderAddr"));
                }
            }
        }

        public bool IsInventory
        {
            get { return IsInventory_; }
            set
            {
                if (value != IsInventory_)
                {
                    IsInventory_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("IsInventory"));
                }
            }
        }

        public bool IsReadTag
        {
            get { return IsReadTag_; }
            set
            {
                if (value != IsReadTag_)
                {
                    IsReadTag_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("IsReadTag"));
                }
            }
        }

        public string ModType
        {
            get { return ModType_; }
            set
            {
                if (value != ModType_)
                {
                    ModType_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("ModType"));
                }
            }
        }

        public string BoardType
        {
            get { return BoardType_; }
            set
            {
                if (value != BoardType_)
                {
                    BoardType_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("BoardType"));
                }
            }
        }

        public string SoftwareVer
        {
            get { return SoftwareVer_; }
            set
            {
                if (value != SoftwareVer_)
                {
                    SoftwareVer_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("SoftwareVer"));
                }
            }
        }
        public string MainboardVer
        {
            get { return MainboardVer_; }
            set
            {
                if (value != MainboardVer_)
                {
                    MainboardVer_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("MainboardVer"));
                }
            }
        }

        public string HardwareVer
        {
            get { return HardwareVer_; }
            set
            {
                if (value != HardwareVer_)
                {
                    HardwareVer_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("HardwareVer"));
                }
            }
        }

        public bool IsConnect
        {
            get { return IsConnect_; }
            set
            {
                if (value != IsConnect_)
                {
                    IsConnect_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("IsConnect"));
                }
            }
        }

        public int Profile
        {
            get { return Profile_; }
            set
            {
                if (value != Profile_)
                {
                    Profile_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("Profile"));
                }
            }
        }
    }
}
