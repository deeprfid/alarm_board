using ModuleTech;
using ReaderManager.Models;
using System;
using System.Globalization;
using System.Collections.Generic;
using System.ComponentModel;
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
using System.Text.RegularExpressions;

namespace ReaderManager
{
    /// <summary>
    /// ActiveModeView.xaml 的交互逻辑
    /// </summary>
    public partial class ActiveModeView : UserControl
    {
        public ActiveModeJson json;
        MainWindow rootWnd;
        public ActiveModeView()
        {
            InitializeComponent();
            json = new ActiveModeJson();
            rootWnd= ReaderParamsViewModel.GetMainWindow();
            rootWnd.AllViews["ActiveModeView"] = this;
            DataContext = json;
        }

        private void btnGet_Click(object sender, RoutedEventArgs e)
        {
            if (!Common.IsIPAddress(IpString.Text))
            {
                rootWnd.TipMessage(LangResouorce.GetText("Msg_Space") +LangResouorce.GetText("Msg_ParamSettings_ParamInter_IpFormatErr"));
                return;
            }
            Application.Current.Properties["Ip"] = IpString.Text;
            App.ConnIp = IpString.Text;

            try
            {
                var ss = Reader.BoardEECommand(IpString.Text, 34, 1000, new byte[1]);
                byte[] content = new byte[2];
                JsonModel.ConvertByteToJonsModel(Reader.BoardEECommand(IpString.Text, 36, 1000, content), out json);
                json.IsGpi_trigger = json.Gpi_trigger != null;

                if (ss[0] == 0x01)
                    json.workPattern = LangResouorce.GetText("ActiveMode_WorkState-1");
                else if (ss[0] == 0x03)
                    json.workPattern = LangResouorce.GetText("ActiveMode_WorkState-3");

                DataContext = json;
            }
            catch (Exception ex)
            {
                rootWnd.TipMessage(LangResouorce.GetText("Msg_Space") +ex.ToString());
            }
        }

        private void btnSet_Click(object sender, RoutedEventArgs e)
        {
            if (!Common.IsIPAddress(IpString.Text))
            {
                rootWnd.TipMessage(LangResouorce.GetText("Msg_Space") + LangResouorce.GetText("Msg_ParamSettings_ParamInter_IpFormatErr"));
                return;
            }
            Application.Current.Properties["Ip"] = IpString.Text;
            App.ConnIp = IpString.Text;
            try
            {
                //Gpi部分
                List<condModel> cond1List = new List<condModel>();
                List<condModel> cond2List = new List<condModel>();
                for (int i = 1; i <= 4; i++)
                {
                    CheckBox GpiCheckBox = (CheckBox)FindName($"GpiCheckBox{i}");

                    if (GpiCheckBox == null || GpiCheckBox.IsChecked != true)
                        continue;

                    condModel condModel = new condModel();
                    System.Windows.Controls.Primitives.ToggleButton GpiToggleButton = (System.Windows.Controls.Primitives.ToggleButton)FindName($"GpiToggleButton{i}");
                    condModel.Id = i > 2 ? i - 2 : i;
                    condModel.State = GpiToggleButton.IsChecked == true ? 1 : 0;

                    if (i <= 2)
                        cond1List.Add(condModel);
                    else
                        cond2List.Add(condModel);
                }

                if (cond1List.Count > 0 || cond2List.Count > 0)
                {
                    if (!json.IsGpi_trigger)
                        json.Gpi_trigger = new gpi_TriggerModel();

                    json.Gpi_trigger.Cond_1 = JsonModel.ConvertListToArray(cond1List);
                    json.Gpi_trigger.Cond_2 = JsonModel.ConvertListToArray(cond2List);
                }


                //Gpo部分
                List<gpo_ActModel> gpo_ActModelList = new List<gpo_ActModel>();
                for (int i = 1; i <= 5; i++)
                {
                    CheckBox GpoCheckBox = (CheckBox)FindName($"GpoCheckBox{i}");
                    if (GpoCheckBox == null || GpoCheckBox.IsChecked != true)
                        continue;

                    gpo_ActModel gpo_ActModel = new gpo_ActModel();
                    TextBox GpoTextBox = (TextBox)FindName($"GpoTextBox{i}");
                    System.Windows.Controls.Primitives.ToggleButton GpoToggleButton = (System.Windows.Controls.Primitives.ToggleButton)FindName($"GpoToggleButton{i}");
                    gpo_ActModel.Dur = Convert.ToInt32(GpoTextBox.Text);
                    gpo_ActModel.State = GpoToggleButton.IsChecked == true ? 1 : 0;
                    gpo_ActModel.Id = i;
                    gpo_ActModelList.Add(gpo_ActModel);
                }
                json.Gpo_act = JsonModel.ConvertListToArray(gpo_ActModelList);

                List<int> listEvents = new List<int>();
                List<int> listTag_json_format = new List<int>();
                for (int i = 0; i <= 8; i++)
                {
                    CheckBox EventsCheckBox = (CheckBox)FindName($"Events{i}");
                    if (EventsCheckBox != null && EventsCheckBox.IsChecked == true)
                    {
                        listEvents.Add(i);
                    }

                    CheckBox Tag_json_formatCheckBox = (CheckBox)FindName($"Tag_json_format{i}");
                    if (Tag_json_formatCheckBox != null && Tag_json_formatCheckBox.IsChecked == true)
                    {
                        listTag_json_format.Add(i);
                    }
                }
                json.Events = JsonModel.ConvertListToArray(listEvents);
                json.Tag_json_format = JsonModel.ConvertListToArray(listTag_json_format);

                if(json.Tag_json_format==null)
                    json.Tag_json_format=new int[0];

                //rootWnd.TipMessage(LangResouorce.GetText("Msg_PlsSelect") + LangResouorce.GetText("Msg_Space") +
                //LangResouorce.GetText("ParamSettings_ParamAdvance_hoptab"));

                var Bytes = JsonModel.ConvertModelToJsonBytes(json);
               
                Reader.BoardEECommand(IpString.Text, 37, 1000, Bytes);
                rootWnd.TipMessage(LangResouorce.GetText("Msg_TagOp_TagOpOK"));
            }
            catch (Exception ex)
            {
                rootWnd.TipMessage(LangResouorce.GetText("Msg_Space") + ex.ToString());
            }
        }

        private void ComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            json.IsTcpOrHttpOrMqtt = json.IsTcpOrHttpOrMqtt;
            json.IsWiegand = json.IsWiegand;
        }

        private void ComboBox_SelectionChanged_1(object sender, SelectionChangedEventArgs e)
        {
            //json.IsHttp = json.IsHttp;
            //json.IsTcp = json.IsTcp;
            //json.IsMqtt = json.IsMqtt;
        }
        public int SelectedValue
        {
            get { return json.Events.FirstOrDefault(); }
            set
            {
                if (json.Events.Contains(value))
                    json.Events = new[] { value };
                else
                    json.Events = new int[0];
            }
        }

        private void btnClear_Click(object sender, RoutedEventArgs e)
        {
            if (!Common.IsIPAddress(IpString.Text))
            {
                rootWnd.TipMessage(LangResouorce.GetText("Msg_Title") + LangResouorce.GetText("Msg_Space") +
                LangResouorce.GetText("Msg_ParamSettings_ParamInter_IpFormatErr"));
                return;
            }

            try
            {
                var Bytes = new byte[2];
                Bytes[0] = 0x00;
                Bytes[1] = 0x50;
                Reader.BoardEECommand(IpString.Text, 32, 1000, Bytes);

                rootWnd.TipMessage(LangResouorce.GetText("Msg_TagOp_TagOpOK"));
            }
            catch (Exception)
            {
                rootWnd.TipMessage(LangResouorce.GetText("Msg_TagOp_TagOpFailed"));
                throw;
            }
        }

        private void btnswitchover_Click(object sender, RoutedEventArgs e)
        {
            if (!Common.IsIPAddress(IpString.Text))
            {
                rootWnd.TipMessage(LangResouorce.GetText("Msg_Title") + LangResouorce.GetText("Msg_Space") +
                LangResouorce.GetText("Msg_ParamSettings_ParamInter_IpFormatErr"));
                return;
            }
            var Bytes = new byte[1];
            if (json.workPattern == LangResouorce.GetText("ActiveMode_WorkState-1"))
                Bytes[0] = 0x03;
            else if (json.workPattern == LangResouorce.GetText("ActiveMode_WorkState-3"))
                Bytes[0] = 0x01;
            else
                return;
            
            Reader.BoardEECommand(IpString.Text, 35, 1000, Bytes);
            rootWnd.TipMessage(LangResouorce.GetText("Msg_TagOp_TagOpOK"));
        }

        private void Grid_Loaded(object sender, RoutedEventArgs e)
        {
            IpString.Text = (string)Application.Current.Properties["Ip"];
        }

        private void IpString_GotFocus(object sender, RoutedEventArgs e)
        {
            if (IpString.Text == String.Empty)
            {
                IpString.Text = "192.168.1.100";
            }
        }
        //btnswitchover_Click
    }
    // 布尔转换器，将bool值转换为Visibility枚举值
    public class BooleanToVisibilityConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is bool boolValue)
            {
                return boolValue ? Visibility.Visible : Visibility.Collapsed;
            }
            return Visibility.Collapsed;
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            throw new NotImplementedException();
        }
    }
    public class ArrayContainsValueConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {

            string parameterString = parameter.ToString();
            if (parameterString.IndexOf('_') == -1)
                return null;

            var names = parameterString.Split('_');
            switch (names[0])
            {
                case "CheckBox":
                    if (value is int[] array)
                    {
                        return array.Contains(int.Parse(names[1].ToString()));
                    }
                    return false;
                case "GpoCheckBox":
                    if (value is gpo_ActModel[] gpo_ActModel)
                    {
                        return gpo_ActModel.Any(r => r.Id == int.Parse(names[1].ToString()));
                    }
                    return false;
                case "GpoTextBox":
                    if (value is gpo_ActModel[] gpo_ActModel1)
                    {
                        if (!gpo_ActModel1.Any(r => r.Id == int.Parse(names[1].ToString())))
                            return null;

                        return gpo_ActModel1.Where(r => r.Id == int.Parse(names[1].ToString())).FirstOrDefault().Dur;
                    }
                    return null;
                case "GpoToggleButton":
                    if (value is gpo_ActModel[] gpo_ActModel2)
                    {
                        if (!gpo_ActModel2.Any(r => r.Id == int.Parse(names[1].ToString())))
                            return false;

                        return gpo_ActModel2.Where(r => r.Id == int.Parse(names[1].ToString())).FirstOrDefault().State != 0;
                    }
                    return false;
                case "GpiCheckBox":
                    if (value is condModel[] condModel1)
                    {
                        return condModel1.Any(r => r.Id == int.Parse(names[1].ToString()));
                    }
                    return false;
                case "GpiToggleButton":
                    if (value is condModel[] condModel2)
                    {
                        if (!condModel2.Any(r => r.Id == int.Parse(names[1].ToString())))
                            return false;

                        return condModel2.Where(r => r.Id == int.Parse(names[1].ToString())).FirstOrDefault().State != 0;
                    }
                    return false;
                default:
                    return null;
            }
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            throw new NotSupportedException();
        }
    }

    public class RangeValidationRule : ValidationRule
    {
        public int MinValue { get; set; }
        public int MaxValue { get; set; }

        public override ValidationResult Validate(object value, CultureInfo cultureInfo)
        {
            if (value is string strValue && int.TryParse(strValue, out int intValue))
            {
                if (intValue >= MinValue && intValue <= MaxValue)
                {
                    return ValidationResult.ValidResult;
                }
            }

            return new ValidationResult(false, $"请输入范围在 {MinValue} 到 {MaxValue} 之间的值。");
        }
    }
}
