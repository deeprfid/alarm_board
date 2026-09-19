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
using ModuleTech;
using ModuleTech.Gen2;
using ReaderManager.Models;

namespace ReaderManager
{
    /// <summary>
    /// TagOpView.xaml 的交互逻辑
    /// </summary>
    public partial class TagOpView : UserControl
    {
        public TagOpView()
        {
            InitializeComponent();
            rpViewModel = ReaderParamsViewModel.GetReaderParamsViewModel();
            rootWnd = ReaderParamsViewModel.GetMainWindow();
            CtrlListSources.AddComboBox(cbbLockType);
            CtrlListSources.AddComboBox(cbbOpType);
            DataContext = rpViewModel;
        }

        ReaderParamsViewModel rpViewModel = null;
        private void UserControl_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            //Debug.WriteLine("this.Width:" + rootWnd.ActualWidth + " userCotlSize:" + mainWndSize);
            foreach (RowDefinition rf in gdMain.RowDefinitions)
            {
                if ((string)rf.Tag != "NoChange")
                    rf.Height = new GridLength(rf.MinHeight * (rootWnd.ActualHeight / rootWnd.mainWndHeightSize));
            }
            tbResult.Width = tbResult.MinWidth * (rootWnd.ActualWidth / rootWnd.mainWndSize);
        }

        MainWindow rootWnd;

        private void UserControl_Loaded(object sender, RoutedEventArgs e)
        {
            foreach (RowDefinition rf in gdMain.RowDefinitions)
            {
                if ((string)rf.Tag != "NoChange")
                    rf.Height = new GridLength(rf.MinHeight * (rootWnd.ActualHeight / rootWnd.mainWndHeightSize));
            }
            tbResult.Width = tbResult.MinWidth * (rootWnd.ActualWidth / rootWnd.mainWndSize);
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
        private void btnRun_Click(object sender, RoutedEventArgs e)
        {
            uint passwd = 0;
            if (rpViewModel.TagOpType == -1)
            {
                rootWnd.TipMessage(LangResouorce.GetText("Msg_PlsSelect") + LangResouorce.GetText("Msg_Space") +
                     LangResouorce.GetText("TagOpView_OpType"));
                return;
            }
            if (rpViewModel.TagOpAnt == -1)
            {
                rootWnd.TipMessage(LangResouorce.GetText("Msg_PlsSelect") + LangResouorce.GetText("Msg_Space") +
                     LangResouorce.GetText("TagOpView_OpAnt"));
                return;
            }
            if (rpViewModel.TagOpAccessPwd.Trim() != string.Empty)
            {
                if (rpViewModel.TagOpAccessPwd.Trim().Length != 8)
                {
                    rootWnd.TipMessage(LangResouorce.GetText("ParamSettings_ParamInv_adddata_combobox_pwd") +
                                           LangResouorce.GetText("Msg_Validation_is") +
                                           LangResouorce.GetText("Msg_Validation_InvalidVal"));
                    return;
                }

                else
                {
                    if (ValidatioAlgorithm.IsValidHexstr(rpViewModel.TagOpAccessPwd.Trim(), 1000) != 0)
                    {
                        rootWnd.TipMessage(LangResouorce.GetText("ParamSettings_ParamInv_adddata_combobox_pwd") +
                          LangResouorce.GetText("Msg_Validation_is") +
                          LangResouorce.GetText("Msg_Validation_InvalidVal"));
                        return;
                    }

                }
                passwd = uint.Parse(rpViewModel.TagOpAccessPwd.Trim(), System.Globalization.NumberStyles.AllowHexSpecifier);
            }

            if (rpViewModel.TagOpType == 0 || rpViewModel.TagOpType == 1)
            {
                if (rpViewModel.TagOpBank == -1)
                {
                    rootWnd.TipMessage(LangResouorce.GetText("Msg_PlsSelect") + LangResouorce.GetText("Msg_Space") +
                        "Bank");
                    return;
                }
                int startaddr = -1;
                if (ValidatioAlgorithm.validNumberParam(rpViewModel.TagOpStartAddr,
                    "ParamSettings_ParamInv_adddata_combobox_startaddr_hint", 0, 10000,
                    out startaddr) != 0)
                    return;

                if (rpViewModel.TagOpAccessPwd.Trim() != string.Empty)
                {
                    if (rpViewModel.TagOpAccessPwd.Trim().Length != 8)
                    {
                        rootWnd.TipMessage(LangResouorce.GetText("ParamSettings_ParamInv_adddata_combobox_pwd") +
                                               LangResouorce.GetText("Msg_Validation_is") +
                                               LangResouorce.GetText("Msg_Validation_InvalidVal"));
                        return;
                    }

                    else
                    {
                        if (ValidatioAlgorithm.IsValidHexstr(rpViewModel.TagOpAccessPwd.Trim(), 1000) != 0)
                        {
                            rootWnd.TipMessage(LangResouorce.GetText("ParamSettings_ParamInv_adddata_combobox_pwd") +
                              LangResouorce.GetText("Msg_Validation_is") +
                              LangResouorce.GetText("Msg_Validation_InvalidVal"));
                            return;
                        }

                    }
                    passwd = uint.Parse(rpViewModel.TagOpAccessPwd.Trim(), System.Globalization.NumberStyles.AllowHexSpecifier);                    
                }
                else if (rpViewModel.TagOpType == 3)
                {
                    rootWnd.TipMessage(LangResouorce.GetText("ParamSettings_ParamInv_adddata_combobox_pwd") +
                       LangResouorce.GetText("Msg_Validation_is") +
                       LangResouorce.GetText("Msg_Validation_InvalidEmpty"));
                    return;
                }

                if (rpViewModel.TagOpType == 0)
                {
                    int blkcnt = -1;
                    if (ValidatioAlgorithm.validNumberParam(rpViewModel.TagOpBlkCnt,
                        "TagOpView_PureReadBlkCnt", 1, 10000,
                        out blkcnt) != 0)
                        return;

                }
                if (rpViewModel.TagOpType == 1 || rpViewModel.TagOpType == 2)
                {
                    int res = ValidatioAlgorithm.IsValidHexstr(rpViewModel.TagOpHexData.Trim(), 16384);

                    if (res == -3)
                    {
                        rootWnd.TipMessage(LangResouorce.GetText("TagOpView_PureWriteData") +
                           LangResouorce.GetText("Msg_Validation_is") +
                           LangResouorce.GetText("Msg_Validation_InvalidEmpty"));
                        return;
                    }

                    else if (res == -1 || rpViewModel.TagOpHexData.Trim().Length % 4 != 0)
                    {
                        rootWnd.TipMessage(LangResouorce.GetText("TagOpView_PureWriteData") +
                            LangResouorce.GetText("Msg_Validation_is") +
                            LangResouorce.GetText("Msg_Validation_InvalidVal") +
                            LangResouorce.GetText("Msg_Period") +
                            LangResouorce.GetText("Msg_Space") +
                            LangResouorce.GetText("Msg_TagOp_RightDataToWrite"));
                        return;
                    }
                }
                else if (rpViewModel.TagOpType == 3)
                {
                    if (rpViewModel.TagOpLockObj == -1)
                    {
                        rootWnd.TipMessage(LangResouorce.GetText("Msg_PlsSelect") + LangResouorce.GetText("Msg_Space") +
                            LangResouorce.GetText("TagOpView_LockObj"));
                        return;
                    }
                    if (rpViewModel.TagOpLockType == -1)
                    {
                        rootWnd.TipMessage(LangResouorce.GetText("Msg_PlsSelect") + LangResouorce.GetText("Msg_Space") +
                            LangResouorce.GetText("TagOpView_LockType"));
                        return;
                    }

                }
                else if (rpViewModel.TagOpType == 4)
                {
                    if (rpViewModel.TagOpKillPwd.Trim() != string.Empty)
                    {
                        if (rpViewModel.TagOpKillPwd.Trim().Length != 8)
                        {
                            rootWnd.TipMessage(LangResouorce.GetText("TagOpView_KillPwd") +
                                                   LangResouorce.GetText("Msg_Validation_is") +
                                                   LangResouorce.GetText("Msg_Validation_InvalidVal"));
                            return;
                        }

                        else
                        {
                            if (ValidatioAlgorithm.IsValidHexstr(rpViewModel.TagOpKillPwd.Trim(), 1000) != 0)
                            {
                                rootWnd.TipMessage(LangResouorce.GetText("TagOpView_KillPwd") +
                                  LangResouorce.GetText("Msg_Validation_is") +
                                  LangResouorce.GetText("Msg_Validation_InvalidVal"));
                                return;
                            }

                        }
                        passwd = uint.Parse(rpViewModel.TagOpKillPwd.Trim(), System.Globalization.NumberStyles.AllowHexSpecifier);
                    }
                    else
                    {
                        rootWnd.TipMessage(LangResouorce.GetText("TagOpView_KillPwd") +
                           LangResouorce.GetText("Msg_Validation_is") +
                           LangResouorce.GetText("Msg_Validation_InvalidEmpty"));
                        return;
                    }
                }
            }

            try
            {
                rpViewModel.ModReader.ParamSet("AccessPassword", passwd);
                rpViewModel.ModReader.ParamSet("TagopAntenna", rpViewModel.TagOpAnt + 1);

                if (rpViewModel.TagOpType == 0)
                {
                    ushort[] readdata = rpViewModel.ModReader.ReadTagMemWords(
                        rpViewModel.tfViewModel.GetTagFilter(2),
                        (MemBank)rpViewModel.TagOpBank, int.Parse(rpViewModel.TagOpStartAddr.Trim())
                         , int.Parse(rpViewModel.TagOpBlkCnt.Trim()));
                    string readdatastr = "";
                    for (int i = 0; i < readdata.Length; ++i)
                        readdatastr += readdata[i].ToString("X4");

                    rpViewModel.TagOpHexData = readdatastr;
                }
                else if (rpViewModel.TagOpType == 1)
                {
                    ushort[] writedata = new ushort[rpViewModel.TagOpHexData.Trim().Length / 4];
                    for (int a = 0; a < writedata.Length; ++a)
                        writedata[a] = ushort.Parse(rpViewModel.TagOpHexData.Trim().Substring(a * 4, 4), 
                            System.Globalization.NumberStyles.AllowHexSpecifier);
                    rpViewModel.ModReader.WriteTagMemWords(rpViewModel.tfViewModel.GetTagFilter(2),
                         (MemBank)rpViewModel.TagOpBank, int.Parse(rpViewModel.TagOpStartAddr.Trim())
                         , writedata);                    
                }
                else if (rpViewModel.TagOpType == 2)
                {
                    rpViewModel.ModReader.WriteTag(rpViewModel.tfViewModel.GetTagFilter(2), 
                        new TagData(rpViewModel.TagOpHexData.Trim()));
                }
                else if (rpViewModel.TagOpType == 3)
                {
                    Gen2LockAct[] act = new Gen2LockAct[1];
                    switch (rpViewModel.TagOpLockObj)
                    {
                        case 0:
                            {
                                if (rpViewModel.TagOpLockType == 0)
                                    act[0] = Gen2LockAct.ACCESS_UNLOCK;
                                else if (rpViewModel.TagOpLockType == 1)
                                    act[0] = Gen2LockAct.ACCESS_LOCK;
                                else if (rpViewModel.TagOpLockType == 2)
                                    act[0] = Gen2LockAct.ACCESS_PERMALOCK;
                                break;
                            }
                        case 1:
                            {
                                if (rpViewModel.TagOpLockType == 0)
                                    act[0] = Gen2LockAct.KILL_UNLOCK;
                                else if (rpViewModel.TagOpLockType == 1)
                                    act[0] = Gen2LockAct.KILL_LOCK;
                                else if (rpViewModel.TagOpLockType == 2)
                                    act[0] = Gen2LockAct.KILL_PERMALOCK;
                                break;
                            }
                        case 2:
                            {
                                if (rpViewModel.TagOpLockType == 0)
                                    act[0] = Gen2LockAct.EPC_UNLOCK;
                                else if (rpViewModel.TagOpLockType == 1)
                                    act[0] = Gen2LockAct.EPC_LOCK;
                                else if (rpViewModel.TagOpLockType == 2)
                                    act[0] = Gen2LockAct.EPC_PERMALOCK;
                                break;
                            }
                        case 3:
                            {
                                if (rpViewModel.TagOpLockType == 0)
                                    act[0] = Gen2LockAct.TID_UNLOCK;
                                else if (rpViewModel.TagOpLockType == 1)
                                    act[0] = Gen2LockAct.TID_LOCK;
                                else if (rpViewModel.TagOpLockType == 2)
                                    act[0] = Gen2LockAct.TID_PERMALOCK;
                                break;
                            }
                        case 4:
                            {
                                if (rpViewModel.TagOpLockType == 0)
                                    act[0] = Gen2LockAct.USER_UNLOCK;
                                else if (rpViewModel.TagOpLockType == 1)
                                    act[0] = Gen2LockAct.USER_LOCK;
                                else if (rpViewModel.TagOpLockType == 2)
                                    act[0] = Gen2LockAct.USER_PERMALOCK;
                                break;
                            }
                    }
                    rpViewModel.ModReader.LockTag(rpViewModel.tfViewModel.GetTagFilter(2), new Gen2LockAction(act));
                }
                else if (rpViewModel.TagOpType == 4)
                {
                    rpViewModel.ModReader.KillTag(rpViewModel.tfViewModel.GetTagFilter(2), passwd);
                }

                    rootWnd.TipSuccess(LangResouorce.GetText("Msg_TagOp_TagOpOK"));
             //   rpViewModel.TagOpHexData = LangResouorce.GetText("Msg_TagOp_TagOpOK");

            }
            catch (Exception ex)
            {
                rootWnd.TipApiFailed(LangResouorce.GetText("Msg_TagOp_TagOpFailed"), ex);
            }
        }

        private void ComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (rpViewModel.TagOpBank == 1)
                rpViewModel.TagOpStartAddr = "2";
            else
                rpViewModel.TagOpStartAddr = "0";
        }
    }
}
