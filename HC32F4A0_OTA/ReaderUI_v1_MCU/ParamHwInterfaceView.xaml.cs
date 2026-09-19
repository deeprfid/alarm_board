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
using ModuleLibrary;
using ModuleTech;
using ReaderManager.Models;

namespace ReaderManager
{
    /// <summary>
    /// ParamCommonView.xaml 的交互逻辑
    /// </summary>
    public partial class ParamHwInterfaceView : UserControl
    {
        public ParamHwInterfaceView()
        {
            InitializeComponent();
            rootWnd = ReaderParamsViewModel.GetMainWindow();
            rpViewModel = ReaderParamsViewModel.GetReaderParamsViewModel();
            CtrlListSources.AddComboBox(cbbKeyType);
            DataContext = rpViewModel;
            rootWnd.AllViews["ParamHwInterfaceView"] = this;
        }
        ReaderParamsViewModel rpViewModel = null;
        private void UserControl_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            foreach (RowDefinition rf in gdMain.RowDefinitions)
            {
                rf.Height = new GridLength(rf.MinHeight * (rootWnd.ActualHeight / rootWnd.mainWndHeightSize));
            }
        }

        public int SetParams()
        {
            if (rpViewModel.EnableIp)
            {
                if (rpViewModel.Ip.Trim() == string.Empty)
                {
                    rootWnd.TipMessage(LangResouorce.GetText("Msg_PlsInput") + LangResouorce.GetText("Msg_Space") +
                      LangResouorce.GetText("ParamSettings_ParamInter_ipaddr"));
                    return -1;
                }

                if (rpViewModel.SubnetMask.Trim() == string.Empty)
                {
                    rootWnd.TipMessage(LangResouorce.GetText("Msg_PlsInput") + LangResouorce.GetText("Msg_Space") +
                      LangResouorce.GetText("ParamSettings_ParamInter_subnetmask"));
                    return -1;
                }

                if (rpViewModel.Gateway.Trim() == string.Empty)
                {
                    rootWnd.TipMessage(LangResouorce.GetText("Msg_PlsInput") + LangResouorce.GetText("Msg_Space") +
                      LangResouorce.GetText("ParamSettings_ParamInter_gateway"));
                    return -1;
                }

                ReaderIPInfo ipinfo = null;
                try
                {
                    ipinfo = ReaderIPInfo.Create(rpViewModel.Ip.Trim(), rpViewModel.SubnetMask.Trim(),
                        rpViewModel.Gateway.Trim());
                }
                catch
                {
                    rootWnd.TipMessage(LangResouorce.GetText("Msg_ParamSettings_ParamInter_IpFormatErr"));
                    return -1;
                }

   
                try
                {
                    if (rpViewModel.EnableWireless)
                    {
                        ReaderIPInfo_Ex.NetType type = ReaderIPInfo_Ex.NetType.NetType_None;
                        ReaderIPInfo_Ex.WifiSetting wifi = null;

                        if (rpViewModel.IsWirelessOn)
                        {
                            if (rpViewModel.SSID.Trim() == string.Empty)
                            {
                                rootWnd.TipMessage(LangResouorce.GetText("Msg_PlsInput") + LangResouorce.GetText("Msg_Space") + "SSID");
                                return -1;
                            }

                            if (rpViewModel.AuthMode == -1)
                            {
                                rootWnd.TipMessage(LangResouorce.GetText("Msg_PlsSelect") + LangResouorce.GetText("Msg_Space") +
                                    LangResouorce.GetText("ParamSettings_ParamInter_authmode"));
                                return -1;
                            }

                            if (rpViewModel.KeyType == -1)
                            {
                                rootWnd.TipMessage(LangResouorce.GetText("Msg_PlsSelect") + LangResouorce.GetText("Msg_Space") +
                                    LangResouorce.GetText("ParamSettings_ParamInter_keytype"));
                                return -1;
                            }
                            if (rpViewModel.AuthMode == 3 || rpViewModel.AuthMode == 4)
                            {
                                if (rpViewModel.KeyType == 1)
                                {
                                    rootWnd.TipMessage(LangResouorce.GetText("Msg_ParamSettings_ParamInter_KeyTypeLimit"));
                                    return -1;
                                }
                            }

                            ReaderIPInfo_Ex.WifiSetting.AuthMode auth = (ReaderIPInfo_Ex.WifiSetting.AuthMode)(rpViewModel.AuthMode + 1);
                            if (rpViewModel.AuthMode == 0)
                                wifi = new ReaderIPInfo_Ex.WifiSetting(auth, rpViewModel.SSID.Trim(),
                                    ReaderIPInfo_Ex.WifiSetting.KeyType.KeyType_NONE, null);
                            else
                            {
                                wifi = new ReaderIPInfo_Ex.WifiSetting(auth, rpViewModel.SSID.Trim(),
                                    (ReaderIPInfo_Ex.WifiSetting.KeyType)(rpViewModel.KeyType + 1), rpViewModel.Key.Trim());
                            }

                            type = ReaderIPInfo_Ex.NetType.NetType_Wifi;
                        }
                        else
                            type = ReaderIPInfo_Ex.NetType.NetType_Ethernet;

                        ReaderIPInfo_Ex ininfoex = new ReaderIPInfo_Ex(ipinfo, type, wifi);
                        rpViewModel.ModReader.ParamSet("IPAddressEx", ininfoex);
                    }
                    else
                        rpViewModel.ModReader.ParamSet("IPAddress", ipinfo);
                }
                catch (Exception ex)
                {
                    rootWnd.TipApiFailed(LangResouorce.GetText("Msg_Params_SetFailed"), ex);
                    return -1;
                }
            
            }

            rootWnd.TipSuccess(LangResouorce.GetText("Msg_Params_SetSuccess"));
            return 0;
        }
        public int GetParams(bool isTip = false)
        {
            if (rpViewModel.EnableIp)
            {
                try
                {
                    if (rpViewModel.ModReader.HwDetails.board == Reader.MaindBoard_Type.MAINBOARD_ARM7)
                    {
                        ReaderIPInfo ipinfo = (ReaderIPInfo)rpViewModel.ModReader.ParamGet("IPAddress");
                        rpViewModel.Ip = ipinfo.IP;
                        rpViewModel.SubnetMask = ipinfo.SUBNET;
                        rpViewModel.Gateway = ipinfo.GATEWAY;
                    }
                    else if (rpViewModel.ModReader.HwDetails.board == Reader.MaindBoard_Type.MAINBOARD_ARM9 ||
                        rpViewModel.ModReader.HwDetails.board == Reader.MaindBoard_Type.MAINBOARD_ARM9_WIFI)
                    {
                        ReaderIPInfo_Ex ipinfo = (ReaderIPInfo_Ex)rpViewModel.ModReader.ParamGet("IPAddressEx");
                        rpViewModel.Ip = ipinfo.IPInfo.IP;
                        rpViewModel.SubnetMask = ipinfo.IPInfo.SUBNET;
                        rpViewModel.Gateway = ipinfo.IPInfo.GATEWAY;

                        if (rpViewModel.ModReader.HwDetails.board == Reader.MaindBoard_Type.MAINBOARD_ARM9_WIFI)
                        {
                            if (ipinfo.NType == ReaderIPInfo_Ex.NetType.NetType_Ethernet)
                                rpViewModel.IsWirelessOn = false;
                            else if (ipinfo.NType == ReaderIPInfo_Ex.NetType.NetType_Wifi)
                            {
                                rpViewModel.AuthMode = (int)(ipinfo.Wifi.Auth - 1);
                                rpViewModel.SSID = ipinfo.Wifi.SSID;
                                if (ipinfo.Wifi.KEY != null)
                                    rpViewModel.Key = ipinfo.Wifi.KEY;
                                else
                                    rpViewModel.Key = "";
                                rpViewModel.IsWirelessOn = true;
                                rpViewModel.KeyType = (int)(ipinfo.Wifi.KType - 1);
                            }
                        }
                    }
                    if (isTip)
                        ReaderParamsViewModel.GetMainWindow().TipSuccess(LangResouorce.GetText("Msg_Params_GetSuccess"));
                }
                catch (Exception ex)
                {
                    ReaderParamsViewModel.GetMainWindow().TipApiFailed(LangResouorce.GetText("Msg_Params_GetFailed"), ex);
                    return -1;
                }
            }

            return 0;
        }

        MainWindow rootWnd;
        private void UserControl_Loaded(object sender, RoutedEventArgs e)
        {
            foreach (RowDefinition rf in gdMain.RowDefinitions)
            {
                rf.Height = new GridLength(rf.MinHeight * (rootWnd.ActualHeight / rootWnd.mainWndHeightSize));
            }
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
