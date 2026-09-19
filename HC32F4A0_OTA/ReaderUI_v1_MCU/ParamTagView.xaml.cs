using Newtonsoft.Json;
using ReaderManager.Models;
using System;
using System.Collections.Generic;
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
using System.Collections.ObjectModel;
using System.Timers;
using MahApps.Metro.Controls;
using System.IO;
using System.Threading;
using System.Security.Policy;
using System.Web;
using ThingMagic;
using System.Net.Http.Headers;
using ControlzEx.Controls;
using static System.Windows.Forms.VisualStyles.VisualStyleElement.ProgressBar;
using System.Runtime.InteropServices;
using System.Web.Caching;
using Newtonsoft.Json.Linq;
using System.Windows.Forms;
using System.Drawing.Imaging;
using System.Drawing;
using System.Collections;
using System.Diagnostics;

namespace ReaderManager
{
    /// <summary>
    /// ParamTagView.xaml 的交互逻辑
    /// </summary>
    ///         
    /// 
    public class tagInfo
    {
        public string no { get; set; }
        public string tag { get; set; }
        public string image { get; set; }
        public string flag { get; set; }
    }
    public partial class ParamTagView : System.Windows.Controls.UserControl
    {
        ReaderParamsViewModel rpViewModel = null;
        MainWindow rootWnd;
        private bool bStart = false;
        private System.Timers.Timer myTime;
        private ImageList myList = new ImageList();
        private ObservableCollection<BitmapImage> myListImage = new ObservableCollection<BitmapImage>();

        private ObservableCollection<tagInfo> items = new ObservableCollection<tagInfo>();
        public ParamTagView()
        {
            InitializeComponent();

            rpViewModel = ReaderParamsViewModel.GetReaderParamsViewModel();
            rootWnd = ReaderParamsViewModel.GetMainWindow();
            rootWnd.AllViews["ParamTagView"] = this;

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

            myTime = new System.Timers.Timer();
            myTime.Interval = 300;
            myTime.Elapsed += Timer_Elapsed;


        }


        private async void Timer_Elapsed(object sender, ElapsedEventArgs e)
        {
            string currentTime = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"); // 设置日期和时间的显示格式
            Console.WriteLine(currentTime);
            try
            {
                this.Invoke(new Action(async () =>
                {
                    string strIp = cbDevIp.Text;

                    string strUrl = "http://" + strIp + ":8080/moduleapi/eascfg";

                    string jsonData = "{\"method\":\"upload\",\"option\":\"get\"}";
                    Task<string> returnstr = PostJson(strUrl, jsonData);
                    string result = await returnstr;
                    if (result == string.Empty)
                    {
                        return;
                    }
                    WhitePacket varData = JsonConvert.DeserializeObject<WhitePacket>(result);
                    items.Clear();
                    int iResult = varData.err_code;
                    if (iResult == 0)
                    {

                        Console.WriteLine(LangResouorce.GetText("ParamSettings_eas_getsucc"));

                        int iCount = varData.tagcount;
                        List<string> myCode = varData.epc;


                        for (int i = 0; i < iCount; i++)
                        {
                            string strCode = myCode[i];
                            string[] parts = strCode.Split('-');

                            if (parts.Length == 2) // 确保分割后得到了两个部分  
                            {
                                string strTag = parts[0];
                                int Flag = int.Parse(parts[1]);
                                if (Flag == 0)
                                {
                                    items.Add(new tagInfo { no = Convert.ToString(i), tag = strTag, image = "green.png" , flag = "否"});
                                }
                                else
                                {
                                    items.Add(new tagInfo { no = Convert.ToString(i), tag = strTag, image = "red.png", flag = "是" });
                                }

                                //  items.Add(new tagInfo { no = Convert.ToString(i), tag = strCode, image = myListImage[0] });

                            }
                        }
                        listTag.ItemsSource = items;
                    }

                }));
            }
            catch (Exception ex)
            {
                rootWnd.TipMessage(LangResouorce.GetText("Msg_Space") + ex.ToString());
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
        bool HasGotParams = false;
        private object timer;

        public void ResetView()
        {
            HasGotParams = false;
        }
        public void Leave()
        {
            bStart = false;
            myTime.Close();
        }
        private async void btnClear_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                items.Clear();
                string strIp = cbDevIp.Text;

                string strUrl = "http://" + strIp + ":8080/moduleapi/eascfg";

                string jsonData = "{\"method\":\"upload\",\"option\":\"clear\"}";
                Task<string> returnstr = PostJson(strUrl, jsonData);
                string result = await returnstr;
                if (result == string.Empty)
                {
                    return;
                }

                var varResult = JsonConvert.DeserializeObject<dynamic>(result);
                string strResult = varResult["err_code"];
                Console.WriteLine(strResult);
                if (strResult == "0")
                {

                    Console.WriteLine(LangResouorce.GetText("ParamSettings_eas_setsucc"));
                }
                else
                {
                    Console.WriteLine(LangResouorce.GetText("ParamSettings_eas_setfail"));
                }
            }
            catch( Exception)
            {

            }
     

        }

        private async void btnStart_Click(object sender, RoutedEventArgs e)
        {
            if (bStart)
            {
                return;
            }
            bStart = true;
            items.Clear();
            rpViewModel.IsReadTag = true;
            myTime.Start();
        }
        private async void btnStop_Click(object sender, RoutedEventArgs e)
        {
            bStart = false;
            myTime.Close();
            rpViewModel.IsReadTag = false;
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
                using (HttpClient client = new HttpClient())
                {
                    ServicePointManager.Expect100Continue = false;
                    ServicePointManager.ServerCertificateValidationCallback = delegate { return true; };
                    ServicePointManager.SecurityProtocol = (SecurityProtocolType)192 | (SecurityProtocolType)768 | (SecurityProtocolType)3072 | (SecurityProtocolType)12288;

                    client.DefaultRequestHeaders.Add("Connection", "keep-alive");

                    HttpContent content = new StringContent(jsonData, Encoding.UTF8, "application/json");
                    content.Headers.ContentLength = jsonData.Length;
                    client.Timeout = TimeSpan.FromSeconds(60);
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
