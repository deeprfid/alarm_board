using Newtonsoft.Json;
using ReaderManager.Models;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
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
using Newtonsoft.Json.Linq;
using static System.Windows.Forms.AxHost;
using System.Diagnostics;
using System.Reflection.Emit;

namespace ReaderManager
{
    public class GetMqttPacket
    {
        public string reader_name { get; set; }
        public string op_type { get; set; }
        public int err_code { get; set; }
        public string err_string { get; set; }
        public string result { get; set; }

        public string host { get; set; }
        public string port { get; set; }
        public string user_name { get; set; }
        public string user_pwd { get; set; }

        public string warehouseCode { get; set; }
        public string warehouseType { get; set; }
        public string deviceType { get; set; }
        public string deviceSn { get; set; }
        public string deviceModel { get; set; }
        public string deviceCode { get; set; }
        public string deviceBrand { get; set; }
        public string softVersion { get; set; }
        public string empCode { get; set; }
        public string softName { get; set; }
        public string remark { get; set; }
    }

    public class SetMqttPacket
    {
        public string method { get; set; }
        public string option { get; set; }

        public string host { get; set; }
        public string port { get; set; }
        public string user_name { get; set; }
        public string user_pwd { get; set; }

        public string warehouseCode { get; set; }
        public string warehouseType { get; set; }
        public string deviceType { get; set; }
        public string deviceSn { get; set; }
        public string deviceModel { get; set; }
        public string deviceCode { get; set; }
        public string deviceBrand { get; set; }
        public string softVersion { get; set; }
        public string empCode { get; set; }
        public string softName { get; set; }
        public string remark { get; set; }

    }
    public partial class ParamStoreView : UserControl
    {
        ReaderParamsViewModel rpViewModel = null;
        MainWindow rootWnd;
        private System.Windows.Forms.Timer myTime;

        public ParamStoreView()
        {
            InitializeComponent();

            rpViewModel = ReaderParamsViewModel.GetReaderParamsViewModel();
            rootWnd = ReaderParamsViewModel.GetMainWindow();
            rootWnd.AllViews["ParamStoreView"] = this;

            DataContext = rpViewModel;

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

        }
        public int InitParams(bool isTip = false)
        {
            string currentTime = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"); // 设置日期和时间的显示格式
            Console.WriteLine(currentTime);

            return 0;
        }

        public int GetParams(bool isTip = false)
        {
            return 0;
        }


        private async void btnGet_Click(object sender, RoutedEventArgs e)
        {

            try
            {
                string strIp = cbDevIp.Text;

                string strUrl = "http://" + strIp + ":8080/moduleapi/eascfg";
                string jsonData = "{\"method\":\"mqtt\",\"option\":\"get\"}";

                Task<string> returnstr = PostJson(strUrl, jsonData);
                string result = await returnstr;
                if (result == string.Empty)
                {
                    tbResult.Text = LangResouorce.GetText("ParamSettings_eas_getfail");
                    return;
                }


                GetMqttPacket varData = JsonConvert.DeserializeObject<GetMqttPacket>(result);
                tbAddress.Text = varData.host.ToString();
                tbPort.Text = varData.port.ToString();
                tbUser.Text = varData.user_name.ToString();
                tbPwd.Text = varData.user_pwd.ToString();

                tbStoreCode.Text = varData.warehouseCode.ToString();
                tbStoreType.Text = varData.warehouseType.ToString();
                tbDeviceType.Text = varData.deviceType.ToString();
                tbDeviceSn.Text = varData.deviceCode.ToString();
                tbDeviceModel.Text = varData.deviceModel.ToString();
                
                tbBrand.Text = varData.deviceBrand.ToString();
                tbSoftware.Text = varData.softVersion.ToString();
                tbStaff.Text = varData.empCode.ToString();
                tbSoftname.Text = varData.softName.ToString();
                tbRemark.Text = varData.remark.ToString();

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
        private async void btnSet_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                string currentTime = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"); // 设置日期和时间的显示格式
                Console.WriteLine(currentTime);
                string strIp = cbDevIp.Text;
                string strUrl = "http://" + strIp + ":8080/moduleapi/eascfg";

                SetMqttPacket varData = new SetMqttPacket();
                varData.method = "mqtt";
                varData.option = "set";
                varData.host = tbAddress.Text;
                varData.port = tbPort.Text;
                varData.user_name = tbUser.Text;
                varData.user_pwd = tbPwd.Text;

                
                varData.warehouseCode= tbStoreCode.Text;
                varData.warehouseType = tbStoreType.Text ;
                varData.deviceType = tbDeviceType.Text;
                varData.deviceCode = tbDeviceSn.Text;
                varData.deviceModel = tbDeviceModel.Text;

                varData.deviceBrand = tbBrand.Text ;
                varData.softVersion= tbSoftware.Text;
                varData.empCode = tbStaff.Text;
                varData.softName = tbSoftname.Text;
                varData.remark = tbRemark.Text;

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
                tbResult.Text = LangResouorce.GetText("ParamSettings_eas_setfail");
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
        public static async Task<string> PostJson(string url, string jsonData)
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
    }
}
