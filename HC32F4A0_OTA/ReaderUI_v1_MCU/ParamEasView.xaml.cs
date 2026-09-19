using ReaderManager.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Http.Headers;
using System.Net.Http;
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
using Newtonsoft.Json;
using System.Security.Cryptography;
using System.Diagnostics;
using System.Net;
using System.Timers;
using MahApps.Metro.Controls;
using LiveCharts.Dtos;
using System.Collections;
using ControlzEx.Standard;
using static System.Windows.Forms.AxHost;
using Newtonsoft.Json.Linq;
using System.Collections.ObjectModel;
using System.IO;
using System.Web;



namespace ReaderManager
{
    public class FilterRule
    {
        public int ch_num { get; set; }
        public int ch_status { get; set; }
        public int start_addr { get; set; }
        public int match_len { get; set; }
        public string mask_code { get; set; }
    }

    public class SetPacket
    {
        public int set { get; set; }
        public string tagstoragedays { get; set; }
        public int easflag { get; set; }

        public int radar_range { get; set; }
        public int alarm_volume { get; set; }
        public int tag_read_cnt { get; set; }
        public int alarm_duration { get; set; }
        public int alarm_switch { get; set; }
        public int accumulated_time { get; set; }
        public int accumulated_count { get; set; }
        public int opening_time { get; set; }
        public int closing_time { get; set; }
        public string remark { get; set; }
        public string system_time { get; set; }
        public List<FilterRule> filter_rule { get; set; }

    }
    public class DataPacket
    {
        public string reader_name { get; set; }
        public string op_type { get; set; }
        public int err_code { get; set; }
        public string err_string { get; set; }
        public string result { get; set; }
        public string tagstoragedays { get; set; }
        public int totaltags { get; set; }
        public int totalalarmcnt { get; set; }
        public int radar_range { get; set; }
        public int alarm_volume { get; set; }
        public int tag_read_cnt { get; set; }
        public int alarm_duration { get; set; }
        public int easflag { get; set; }
        public string deviceID { get; set; }
        public string system_time { get; set; }
        public string peoplecount { get; set; }
        public int alarm_switch { get; set; }
        public int accumulated_time { get; set; }
        public int accumulated_count { get; set; }
        public int opening_time { get; set; }
        public int closing_time { get; set; }
        public string remark { get; set; }
        public List<FilterRule> filter_rule { get; set; }
    }
    public class Alarm
    {
        public string time { get; set; }
        public string deviceNo { get; set; }
        public string epc { get; set; }
        public string mem { get; set; }

    }

    public class EpcList
    {
        public string time { get; set; }
        public string deviceNo { get; set; }
        public List<String> epc { get; set; }
        public string mem { get; set; }

    }

    public class AlarmData
    {
        public string reader_name { get; set; }
        public string op_type { get; set; }
        public int err_code { get; set; }
        public string err_string { get; set; }
        public int code { get; set; }
        public string msg { get; set; }
        public int tagcount { get; set; }
        public List<Alarm> data { get; set; }

    }
    /// <summary>
    /// ParamEasView.xaml 的交互逻辑
    /// </summary>
    public partial class ParamEasView : UserControl
    {
        ReaderParamsViewModel rpViewModel = null;
        MainWindow rootWnd;
        public ParamEasView()
        {            
            InitializeComponent();

            Timer timer = new Timer();
            timer.Interval = 1000;
            timer.Elapsed += Timer_Elapsed;
            timer.Start();

         //   long timestamp = 1721219545; // 时间戳

      //      DateTime dateTime = new DateTime(1970, 1, 1, 0, 0, 0, 0, DateTimeKind.Utc)
      //          .AddSeconds(timestamp);

      //      string dateString = dateTime.ToString("yyyy-MM-dd HH:mm:ss");

            string devPath = AppDomain.CurrentDomain.BaseDirectory + "curDev.csv";
            try
            {
                using (StreamReader reader = new StreamReader(devPath))
                {
                    string line;
                    while ((line = reader.ReadLine()) != null)
                    {
                        cbDevIp.Items.Add(line);
                        cbDevIp.Text = line;
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine("An error occurred: " + ex.Message);
            }

            rpViewModel = ReaderParamsViewModel.GetReaderParamsViewModel();
            rootWnd = ReaderParamsViewModel.GetMainWindow();
            rootWnd.AllViews["ParamEasView"] = this;

            DataContext = rpViewModel;
     
        }
        private void Timer_Elapsed(object sender, ElapsedEventArgs e)
        {
            string currentTime = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"); // 设置日期和时间的显示格式

            try
            {
                this.Invoke(new Action(() =>
                {
                    tbCurTime.Text = currentTime;

                }));
            }
            catch (Exception ex)
            {
                rootWnd.TipMessage(LangResouorce.GetText("Msg_Space") + ex.ToString());
            }

        }
        public int InitParams(bool isTip =false)
        {
            string currentTime = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"); // 设置日期和时间的显示格式
            tbCurTime.Text = currentTime;
            Console.WriteLine(currentTime);



            return 0;
        }
        public int GetParams(bool isTip = false)
        {
            return 0;
        }
        public int SetParams(bool isTip = false)
        {
            return 0;
        }
        public int SetPerpetualParams()
        {
            return 0;
        }

        private async void btnReboot_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                string currentTime = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"); // 设置日期和时间的显示格式
                Console.WriteLine(currentTime);
                string strIp = cbDevIp.Text;

                string strUrl = "http://" + strIp + ":8080/moduleapi/reboot";
                string jsonData = "";
                Task<string> returnstr = PostJson(strUrl, jsonData);
                string result = await returnstr;
                if (result == string.Empty)
                {
                    tbResult.Text = LangResouorce.GetText("ParamSettings_eas_rebootfail");
                    return;
                }
                var varResult = JsonConvert.DeserializeObject<dynamic>(result);
                string strResult = varResult["err_code"];
                Console.WriteLine(strResult);
                if (strResult == "0")
                {
                    Console.WriteLine(LangResouorce.GetText("ParamSettings_eas_rebootsucc"));
                    tbResult.Text = LangResouorce.GetText("ParamSettings_eas_rebootsucc");
                }
                else
                {
                    tbResult.Text = LangResouorce.GetText("ParamSettings_eas_rebootfail");
                }
            }
            catch (Exception)
            {
                tbResult.Text = LangResouorce.GetText("ParamSettings_eas_rebootfail");
            }
        }

        private async void btnGet_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                string currentTime = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"); // 设置日期和时间的显示格式
                Console.WriteLine(currentTime);
                string strIp = cbDevIp.Text; // ;App.ConnIp;

                string strUrl = "http://" + strIp + ":8080/moduleapi/eascfg";

                string jsonData = "{\"get\":1,\"system_time\":\"" + currentTime + "\"}";
                Task<string> returnstr = PostJson(strUrl, jsonData);
                string result = await returnstr;
                if (result == string.Empty)
                {
                    tbResult.Text = LangResouorce.GetText("ParamSettings_eas_getfail");
                    return;
                }

                DataPacket varData = JsonConvert.DeserializeObject<DataPacket>(result);
                tbTotal.Text = varData.totaltags.ToString();
                cbFreqDays.Text = varData.tagstoragedays.ToString();
                tbDevId.Text = varData.deviceID.ToString();
                tbATotal.Text = varData.totalalarmcnt.ToString();
                tbVolume.Text = varData.alarm_volume.ToString();
                tbLeida.Text = varData.radar_range.ToString();
                tbATime.Text = varData.alarm_duration.ToString();
                tbThreshold.Text = varData.tag_read_cnt.ToString();
                tbEasFlag.Text = varData.easflag.ToString("X");
                tbPeople.Text = varData.peoplecount.ToString();
                tbSwitch.SelectedIndex = varData.alarm_switch;

                tbTotalTime.Text = varData.accumulated_time.ToString();
                tbCount.Text = varData.accumulated_count.ToString();
                tbStart.SelectedIndex = varData.opening_time;
                tbStop.SelectedIndex = varData.closing_time;
  //              tbRemark.Text = varData.remark.ToString();
                List<FilterRule> myRuleList = varData.filter_rule;
                for (int i = 0; i < myRuleList.Count; i++)
                {
                    FilterRule myRule = myRuleList[i];

                    switch (myRule.ch_num)
                    {
                        case 1:
                            if (myRule.ch_status == 1)
                            {
                                RuleCheck1.IsChecked = true;
                            }
                            else
                            {
                                RuleCheck1.IsChecked = false;
                            }
                            Start1.Text = myRule.start_addr.ToString();
                            MValue1.Text = myRule.match_len.ToString();
                            tbMask1.Text = myRule.mask_code;
                            break;
                        case 2:
                            if (myRule.ch_status == 1)
                            {
                                RuleCheck2.IsChecked = true;
                            }
                            else
                            {
                                RuleCheck2.IsChecked = false;
                            }
                            Start2.Text = myRule.start_addr.ToString();
                            MValue2.Text = myRule.match_len.ToString();
                            tbMask2.Text = myRule.mask_code;
                            break;
                        case 3:
                            if (myRule.ch_status == 1)
                            {
                                RuleCheck3.IsChecked = true;
                            }
                            else
                            {
                                RuleCheck3.IsChecked = false;
                            }
                            Start3.Text = myRule.start_addr.ToString();
                            MValue3.Text = myRule.match_len.ToString();
                            tbMask3.Text = myRule.mask_code;
                            break;
                        case 4:
                            if (myRule.ch_status == 1)
                            {
                                RuleCheck4.IsChecked = true;
                            }
                            else
                            {
                                RuleCheck4.IsChecked = false;
                            }
                            Start4.Text = myRule.start_addr.ToString();
                            MValue4.Text = myRule.match_len.ToString();
                            tbMask4.Text = myRule.mask_code;
                            break;
                        case 5:
                            if (myRule.ch_status == 1)
                            {
                                RuleCheck5.IsChecked = true;
                            }
                            else
                            {
                                RuleCheck5.IsChecked = false;
                            }
                            Start5.Text = myRule.start_addr.ToString();
                            MValue5.Text = myRule.match_len.ToString();
                            tbMask5.Text = myRule.mask_code;
                            break;
                        case 6:
                            if (myRule.ch_status == 1)
                            {
                                RuleCheck6.IsChecked = true;
                            }
                            else
                            {
                                RuleCheck6.IsChecked = false;
                            }
                            Start6.Text = myRule.start_addr.ToString();
                            MValue6.Text = myRule.match_len.ToString();
                            tbMask6.Text = myRule.mask_code;
                            break;
                        case 7:
                            if (myRule.ch_status == 1)
                            {
                                RuleCheck7.IsChecked = true;
                            }
                            else
                            {
                                RuleCheck7.IsChecked = false;
                            }
                            Start7.Text = myRule.start_addr.ToString();
                            MValue7.Text = myRule.match_len.ToString();
                            tbMask7.Text = myRule.mask_code;
                            break;
                        case 8:
                            if (myRule.ch_status == 1)
                            {
                                RuleCheck8.IsChecked = true;
                            }
                            else
                            {
                                RuleCheck8.IsChecked = false;
                            }
                            Start8.Text = myRule.start_addr.ToString();
                            MValue8.Text = myRule.match_len.ToString();
                            tbMask8.Text = myRule.mask_code;
                            break;
                        case 9:
                            if (myRule.ch_status == 1)
                            {
                                RuleCheck9.IsChecked = true;
                            }
                            else
                            {
                                RuleCheck9.IsChecked = false;
                            }
                            Start9.Text = myRule.start_addr.ToString();
                            MValue9.Text = myRule.match_len.ToString();
                            tbMask9.Text = myRule.mask_code;
                            break;
                        case 10:
                            if (myRule.ch_status == 1)
                            {
                                RuleCheck10.IsChecked = true;
                            }
                            else
                            {
                                RuleCheck10.IsChecked = false;
                            }
                            Start10.Text = myRule.start_addr.ToString();
                            MValue10.Text = myRule.match_len.ToString();
                            tbMask10.Text = myRule.mask_code;
                            break;
                        default:
                            break;
                    }
                }

                int iResult = varData.err_code;
                if (iResult == 0)
                {

                    Console.WriteLine(LangResouorce.GetText("ParamSettings_eas_getsucc"));
                    tbResult.Text = LangResouorce.GetText("ParamSettings_eas_getsucc");
                }
                else
                {
                    tbResult.Text = LangResouorce.GetText("ParamSettings_eas_getfail");
                }


            }
            catch (Exception)
            {
               tbResult.Text = LangResouorce.GetText("ParamSettings_eas_getfail");
            }

        }
        private async void btnTest_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                string strIp = cbDevIp.Text;

                string strUrl = "http://" + strIp + ":8080/moduleapi/eascfg";
                string jsonData = "{\"method\":\"gpiotest\"}";

                Task<string> returnstr = PostJson(strUrl, jsonData);
                string result = await returnstr;
                if (result == string.Empty)
                {
                    tbResult.Text = LangResouorce.GetText("ParamSettings_eas_getfail");
                    return;
                }
                var varResult = JsonConvert.DeserializeObject<dynamic>(result);
                string strResult = varResult["err_code"];
                Console.WriteLine(strResult);
                if (strResult == "0")
                {

                    Console.WriteLine(LangResouorce.GetText("ParamSettings_eas_setsucc"));
                    tbResult.Text = LangResouorce.GetText("ParamSettings_eas_setsucc");
                }
                else
                {
                    tbResult.Text = LangResouorce.GetText("ParamSettings_eas_setfail");
                }
            }
            catch (Exception)
            {
                tbResult.Text = LangResouorce.GetText("ParamSettings_eas_setfail");
            }
        }
        private async void btnSet_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                string currentTime = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"); // 设置日期和时间的显示格式
                Console.WriteLine(currentTime);
                string strIp = cbDevIp.Text;// App.ConnIp;
                string strUrl = "http://" + strIp + ":8080/moduleapi/eascfg";

                SetPacket varData = new SetPacket();
                varData.set = 1;
                varData.accumulated_time = Convert.ToInt32(tbTotalTime.Text, 10); 
                varData.accumulated_count = Convert.ToInt32(tbCount.Text, 10);
                varData.opening_time = tbStart.SelectedIndex;
                varData.closing_time = tbStop.SelectedIndex;
                varData.alarm_switch = tbSwitch.SelectedIndex;
     //           varData.remark = tbRemark.Text;
                varData.tagstoragedays = cbFreqDays.Text;
                varData.easflag = Convert.ToInt32(tbEasFlag.Text, 16);
                varData.alarm_volume = Convert.ToInt32(tbVolume.Text, 10);
                varData.radar_range = Convert.ToInt32(tbLeida.Text, 10);
                varData.alarm_duration = Convert.ToInt32(tbATime.Text, 10);
                varData.tag_read_cnt = Convert.ToInt32(tbThreshold.Text, 10);
                varData.system_time = tbCurTime.Text;

                List<FilterRule> myRuleList = new List<FilterRule>();
                for (int i = 1; i < 11; i++)
                {
                    FilterRule myRule = new FilterRule();
                    myRule.ch_num = i;
                    myRule.start_addr = 0;
                    myRule.match_len = 0;
                    myRule.mask_code = "0";
                    switch (i)
                    {
                        case 1:
                            if (RuleCheck1.IsChecked == true)
                            {
                                myRule.ch_status = 1;
                                myRule.start_addr = int.Parse(Start1.Text);
                                myRule.match_len = int.Parse(MValue1.Text);
                                myRule.mask_code = tbMask1.Text;
                            }

                            break;
                        case 2:
                            if (RuleCheck2.IsChecked == true)
                            {
                                myRule.ch_status = 1;
                                myRule.start_addr = int.Parse(Start2.Text);
                                myRule.match_len = int.Parse(MValue2.Text);
                                myRule.mask_code = tbMask2.Text;

                            }

                            break;
                        case 3:
                            if (RuleCheck3.IsChecked == true)
                            {
                                myRule.ch_status = 1;
                                myRule.start_addr = int.Parse(Start3.Text);
                                myRule.match_len = int.Parse(MValue3.Text);
                                myRule.mask_code = tbMask3.Text;

                            }

                            break;
                        case 4:
                            if (RuleCheck4.IsChecked == true)
                            {
                                myRule.ch_status = 1;
                                myRule.start_addr = int.Parse(Start4.Text);
                                myRule.match_len = int.Parse(MValue4.Text);
                                myRule.mask_code = tbMask4.Text;

                            }

                            break;
                        case 5:
                            if (RuleCheck5.IsChecked == true)
                            {
                                myRule.ch_status = 1;
                                myRule.start_addr = int.Parse(Start5.Text);
                                myRule.match_len = int.Parse(MValue5.Text);
                                myRule.mask_code = tbMask5.Text;

                            }

                            break;
                        case 6:
                            if (RuleCheck6.IsChecked == true)
                            {
                                myRule.ch_status = 1;
                                myRule.start_addr = int.Parse(Start6.Text);
                                myRule.match_len = int.Parse(MValue6.Text);
                                myRule.mask_code = tbMask6.Text;
                            }

                            break;
                        case 7:
                            if (RuleCheck7.IsChecked == true)
                            {
                                myRule.ch_status = 1;
                                myRule.start_addr = int.Parse(Start7.Text);
                                myRule.match_len = int.Parse(MValue7.Text);
                                myRule.mask_code = tbMask7.Text;
                            }

                            break;
                        case 8:
                            if (RuleCheck8.IsChecked == true)
                            {
                                myRule.ch_status = 1;
                                myRule.start_addr = int.Parse(Start8.Text);
                                myRule.match_len = int.Parse(MValue8.Text);
                                myRule.mask_code = tbMask8.Text;

                            }

                            break;
                        case 9:
                            if (RuleCheck9.IsChecked == true)
                            {
                                myRule.ch_status = 1;
                                myRule.start_addr = int.Parse(Start9.Text);
                                myRule.match_len = int.Parse(MValue9.Text);
                                myRule.mask_code = tbMask9.Text;

                            }

                            break;
                        case 10:
                            if (RuleCheck10.IsChecked == true)
                            {
                                myRule.ch_status = 1;
                                myRule.start_addr = int.Parse(Start10.Text);
                                myRule.match_len = int.Parse(MValue10.Text);
                                myRule.mask_code = tbMask10.Text;

                            }

                            break;
                        default:
                            break;
                    }
                    myRuleList.Add(myRule);
                }

                varData.filter_rule = myRuleList;
                string jsonData = JsonConvert.SerializeObject(varData);

                Task<string> returnstr = PostJson(strUrl, jsonData);
                string result = await returnstr;
                if (result == string.Empty)
                {
                    tbResult.Text = LangResouorce.GetText("ParamSettings_eas_setfail");
                    return;
                }

                var varResult = JsonConvert.DeserializeObject<dynamic>(result);
                string strResult = varResult["err_code"];
                Console.WriteLine(strResult);
                if (strResult == "0")
                {

                    Console.WriteLine(LangResouorce.GetText("ParamSettings_eas_setsucc"));
                    tbResult.Text = LangResouorce.GetText("ParamSettings_eas_setsucc");
                }
                else
                {
                    tbResult.Text = LangResouorce.GetText("ParamSettings_eas_setfail");
                }
            }
            catch (Exception)
            {
                // 处理异常的代码
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

        public static async Task<string>  Post(string url, string jsonData)
        {
            string responseBody = string.Empty;
            string result = "";

            try
            {
                using (var httpClient = new HttpClient())
                {
                    var httpRequestMessage = new HttpRequestMessage(HttpMethod.Post, new Uri(url))
                    {
                        Version = HttpVersion.Version10,
                 //       Content = httpContent
                    };

                    await httpClient.SendAsync(httpRequestMessage);
                    using (var httpResponseMessage = await httpClient.SendAsync(httpRequestMessage))
                    {
                  //      return await CreateDto(httpResponseMessage);
                    }
                }
               
                byte[] postDataBytes = System.Text.Encoding.UTF8.GetBytes(jsonData);
                HttpWebRequest request = (HttpWebRequest)WebRequest.Create(url);
                request.Method = "POST";
                request.ContentType = "application/x-www-form-urlencoded";
                request.ContentLength = postDataBytes.Length;
                request.ProtocolVersion = HttpVersion.Version10;
                ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls12;

                using (Stream requestStream = request.GetRequestStream())
                {
                    requestStream.Write(postDataBytes, 0, postDataBytes.Length);
                }

                using (HttpWebResponse response = (HttpWebResponse)request.GetResponse())
                {
                    using (Stream responseStream = response.GetResponseStream())
                    {
                        using (StreamReader reader = new StreamReader(responseStream))
                        {
                            string responseString = reader.ReadToEnd();
                            Console.WriteLine(responseString);
                        }
                    }
                }

                return result;
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.ToString());
                System.Diagnostics.Debug.WriteLine("CAUGHT EXCEPTION:");
                System.Diagnostics.Debug.WriteLine(ex);
                return string.Empty;
            }

        }
        public static async Task<string> PostJson(string url, string jsonData)
        {
            string responseBody = string.Empty;
           // responseBody = await Post(url, jsonData);
            try
            {
                var handler = new HttpClientHandler { MaxRequestContentBufferSize = 2147483647 }; 
                using (HttpClient client = new HttpClient(handler))
                {
                    ServicePointManager.Expect100Continue = false;
                    ServicePointManager.ServerCertificateValidationCallback = delegate { return true; };
                    ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls12 | SecurityProtocolType.Tls11 | SecurityProtocolType.Tls;
                    client.Timeout = TimeSpan.FromMinutes(10);


                    HttpContent content = new StringContent(jsonData);
                    
                    content.Headers.ContentType = new System.Net.Http.Headers.MediaTypeHeaderValue("application/json");
                    HttpResponseMessage response = await client.PostAsync(url, content);
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
            catch(Exception ex)
            {
                Console.WriteLine(ex.ToString());
                System.Diagnostics.Debug.WriteLine("CAUGHT EXCEPTION:");
                System.Diagnostics.Debug.WriteLine(ex);
                return string.Empty;
            }

        }

    }
}
