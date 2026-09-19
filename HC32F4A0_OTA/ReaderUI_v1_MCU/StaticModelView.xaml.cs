using ModuleTech;
using Newtonsoft.Json;
using ReaderManager.Models;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Net.Http;
using System.Net;
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
using System.Collections;

namespace ReaderManager
{
    /// <summary>
    /// StaticModelView.xaml 的交互逻辑
    /// </summary>
    public partial class StaticModelView : UserControl
    {
        public StaticModelJson json;
        MainWindow rootWnd;
        public StaticModelView()
        {
            InitializeComponent();
            json = new StaticModelJson();
            rootWnd = ReaderParamsViewModel.GetMainWindow();
            rootWnd.AllViews["StaticModelView"] = this;
            DataContext = json;
        }

        private void btnGet_Click(object sender, RoutedEventArgs e)
        {
            if (!Common.IsIPAddress(IpString.Text))
            {
                rootWnd.TipMessage(LangResouorce.GetText("Msg_Title") + LangResouorce.GetText("Msg_Space") +
                LangResouorce.GetText("Msg_ParamSettings_ParamInter_IpFormatErr"));
                return;
            }
            Application.Current.Properties["Ip"] = IpString.Text;
            App.ConnIp = IpString.Text;
            try
            {
                byte[] content = new byte[2];

                /*     string strUrl = "http://" + IpString.Text + ":8080/moduleapi/paramget";
                     Task<string> returnstr = PostJson(strUrl, "");
                     string result = await returnstr;
                     byte[] byteArray = Encoding.UTF8.GetBytes(result);
                     JsonModel.ConvertByteToJonsModel(byteArray, out json);*/
                JsonModel.ConvertByteToJonsModel(Reader.BoardEECommand(IpString.Text, 30, 1000, content), out json);
                json.IsGpO_enable = json.Gpos != null;
                json.IsTagfilter = json.Tagops_param.Tagfilter != null;
                json.IsBankdata = json.Tagops_param.Bankdata != null;
                json.IsBankdata2 = json.Tagops_param.Bankdata2 != null;
                json.IsBankdata3 = json.Tagops_param.Bankdata3 != null;

                DataContext = json;
            }
            catch (Exception)
            {
                rootWnd.TipMessage(LangResouorce.GetText("Msg_TagOp_TagOpFailed"));
            }
        }

        private void btnSet_Click(object sender, RoutedEventArgs e)
        {
            if (!Common.IsIPAddress(IpString.Text))
            {
                rootWnd.TipMessage(LangResouorce.GetText("Msg_Title") + LangResouorce.GetText("Msg_Space") +
                LangResouorce.GetText("Msg_ParamSettings_ParamInter_IpFormatErr"));
                return;
            }

            Application.Current.Properties["Ip"] = IpString.Text;
            App.ConnIp = IpString.Text;
            try
            {
                if (json.IsGpO_enable)
                {
                    for (int i = 0; i < json.Gpos.Length; i++)
                    {
                        System.Windows.Controls.Primitives.ToggleButton GpoToggleButton = (System.Windows.Controls.Primitives.ToggleButton)FindName($"GpoToggleButton{i + 1}");
                        if (GpoToggleButton == null)
                            continue;

                        GposModel gposModel= new GposModel();
                        gposModel.Id = i+1;
                        gposModel.State = GpoToggleButton.IsChecked == true ? 1 : 0;
                        json.Gpos[i] = gposModel;
                    }
                }
                var Bytes = JsonModel.ConvertModelToJsonBytes(json);

                //       string strBuf = Encoding.UTF8.GetString(Bytes);
                //        string strUrl = "http://" + IpString.Text + ":8080/moduleapi/paramset";
                //       Task<string> returnstr = PostJson(strUrl, strBuf);
                //        string result = await returnstr;

                Reader.BoardEECommand(IpString.Text, 31, 1000, Bytes);

                rootWnd.TipMessage(LangResouorce.GetText("Msg_TagOp_TagOpOK"));
            }
            catch (Exception)
            {
                rootWnd.TipMessage(LangResouorce.GetText("Msg_TagOp_TagOpFailed"));
            }
        }

        private static async Task<string> PostJson(string url, string jsonData)
        {
            string responseBody = string.Empty;
            try
            {
                var handler = new HttpClientHandler { MaxRequestContentBufferSize = 2147483647 };
                using (HttpClient client = new HttpClient(handler))
                {
                    ServicePointManager.Expect100Continue = false;
                    ServicePointManager.ServerCertificateValidationCallback = delegate { return true; };
                    ServicePointManager.SecurityProtocol = (SecurityProtocolType)192 | (SecurityProtocolType)768 | (SecurityProtocolType)3072 | (SecurityProtocolType)12288;

                    client.DefaultRequestHeaders.Add("Connection", "keep-alive");

                    HttpContent content = new StringContent(jsonData, Encoding.UTF8, "application/json");
                    content.Headers.ContentLength = jsonData.Length;

                    client.Timeout = TimeSpan.FromMinutes(5);

                    HttpResponseMessage response = await client.PostAsync(url, content).ConfigureAwait(false);
                    if (response.IsSuccessStatusCode)
                    {
                        response.EnsureSuccessStatusCode();
                        responseBody = await response.Content.ReadAsStringAsync();
                        var result = JsonConvert.DeserializeObject<dynamic>(responseBody);
                    }
                }
                Console.WriteLine(responseBody);
                return responseBody;
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.ToString());
                System.Diagnostics.Debug.WriteLine("CAUGHT EXCEPTION:");
                System.Diagnostics.Debug.WriteLine(ex);
                return string.Empty;
            }

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
    }

    public class ArrayToStringWithUnderscoresConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is int[] array)
            {
                return string.Join(",", array);
            }
            return null;
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is string text)
            {
                string[] stringArray = text.Split(',');
                int[] intArray = Array.ConvertAll(stringArray, int.Parse);
                return intArray;
            }
            return null;
        }
    }

    public class TextBlockConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (parameter.ToString().IndexOf("GpoToggleButton") != -1) {
                var names = parameter.ToString().Split('_');
                if (value is GposModel[] gpo_Model)
                {
                    if (!gpo_Model.Any(r => r.Id == int.Parse(names[1].ToString())))
                        return false;

                    return gpo_Model.Where(r => r.Id == int.Parse(names[1].ToString())).FirstOrDefault().State != 0;
                }
                return false;
            }

            int _value = (int)value;
            if (parameter.ToString() == "Profile")
            {
                if (_value <= 3)
                    return _value;
                else if (_value == 45)
                    return 20;
                else if (_value > 100)
                {
                    if (_value == 101)
                        return 10;
                    if (_value == 103)
                        return 11;
                    if (_value == 105)
                        return 12;
                    if (_value == 107)
                        return 13;
                    if (_value == 111)
                        return 14;
                    if (_value == 112)
                        return 15;
                    if (_value == 113)
                        return 16;
                    if (_value == 115)
                        return 17;
                    if (_value == 203)
                        return 18;
                    if (_value == 220)
                        return 19;
                }
                else
                {
                    return _value - 12;
                }
            }
            else if (parameter.ToString() == "QValue") {
                return _value + 1;
            }
          
            return null;
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            int _value = (int)value;
            if (parameter.ToString() == "Profile") {
                if (_value <= 3)
                    return _value;
                else if (_value == 20)
                    return 45;
                else if (_value > 100)
                {
                    if (_value == 10)
                        return 101;
                    if (_value == 11)
                        return 103;
                    if (_value == 12)
                        return 105;
                    if (_value == 13)
                        return 107;
                    if (_value == 14)
                        return 111;
                    if (_value == 15)
                        return 112;
                    if (_value == 16)
                        return 113;
                    if (_value == 17)
                        return 115;
                    if (_value == 18)
                        return 203;
                    if (_value == 19)
                        return 220;
                }
                else
                {
                    return _value + 12;
                }
            }
            else if (parameter.ToString() == "QValue") {
                return _value - 1;
            }
            
            return 0;
        }
    }

}
