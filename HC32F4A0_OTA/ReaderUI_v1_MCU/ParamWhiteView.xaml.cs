using Newtonsoft.Json.Linq;
using Newtonsoft.Json;
using ReaderManager.Models;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
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
using System.Timers;
using System.Windows.Forms;
using System.IO;
using CsvHelper;
using System.IO.Packaging;
using System.Globalization;
using System.Data;
using LiveCharts.Dtos;
using Xceed.Wpf.Toolkit.PropertyGrid.Attributes;
using static System.Windows.Forms.VisualStyles.VisualStyleElement;


namespace ReaderManager
{
    public class WhitePacket
    {
        public string method { get; set; }
        public int tagcount { get; set; }
        public string createtime { get; set; }

        public int err_code { get; set; }
        public string err_string { get; set; }
        public List<String> epc { get; set; }

    }
    public class WhiteInfo
    {
        public string no { get; set; }
        public string tag { get; set; }

    }

    /// <summary>
    /// ParamWhiteView.xaml 的交互逻辑
    /// </summary>
    public partial class ParamWhiteView : System.Windows.Controls.UserControl
    {
        ReaderParamsViewModel rpViewModel = null;
        MainWindow rootWnd;
        public ParamWhiteView()
        {
            InitializeComponent();
            
            rpViewModel = ReaderParamsViewModel.GetReaderParamsViewModel();
            rootWnd = ReaderParamsViewModel.GetMainWindow();
            rootWnd.AllViews["ParamWhiteView"] = this;

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

        private async void btnReadFile_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                OpenFileDialog openFileDialog = new OpenFileDialog();
                openFileDialog.Filter = "CSV Files (*.csv)|*.csv";
                openFileDialog.Title = "选择CSV文件";

                if (openFileDialog.ShowDialog() == DialogResult.OK)
                {
                    string csvFilePath = openFileDialog.FileName;

                    // 在这里可以使用csvFilePath进行文件读取操作
                    Console.WriteLine("选择的CSV文件路径：" + csvFilePath);

                    ObservableCollection<WhiteInfo> items = new ObservableCollection<WhiteInfo>();                   

                    using (var reader = new StreamReader(csvFilePath))
                    using (var csv = new CsvReader(reader, CultureInfo.InvariantCulture))
                    {
                        // 读取CSV文件的标题行
                        csv.Read();
                        csv.ReadHeader();

                        int iNo = 0;
                        // 读取CSV文件的数据行
                        while (csv.Read())
                        {
                            // 读取每一列的值
                            string column1 = csv.GetField<string>("Tag");
                            items.Add(new WhiteInfo { no = Convert.ToString(iNo++), tag = column1});

                        }
                        listWhite.ItemsSource = items;
                    }
                }
            }
            catch (Exception ex)
            {
                // 处理异常
                Console.WriteLine("发生异常：" + ex.Message);
            }

        }
        private async void btnClear_Click(object sender, RoutedEventArgs e)
        {
            /* v9.82j: 清除 = 确认框 + 清屏 + 下发 cleartag 指令，真正清掉设备白名单 */
            var confirm = System.Windows.MessageBox.Show("确定清除设备上的全部报警名单(白名单)吗？此操作不可恢复！", "确认清除",
                                           System.Windows.MessageBoxButton.YesNo, System.Windows.MessageBoxImage.Warning);
            if (confirm != System.Windows.MessageBoxResult.Yes)
                return;
            string strIp = cbDevIp.Text;
            string strUrl = "http://" + strIp + ":8080/moduleapi/eascfg";
            string jsonData = "{\"method\":\"cleartag\"}";
            try
            {
                Task<string> returnstr = PostJson(strUrl, jsonData);
                string result = await returnstr;
                if (result != string.Empty)
                {
                    var varResult = JsonConvert.DeserializeObject<dynamic>(result);
                    string strErr = Convert.ToString(varResult["err_code"]);
                    if (strErr == "0")
                    {
                        listWhite.ItemsSource = null;
                        listWhite.Items.Clear();
                        tbResult.Text = LangResouorce.GetText("ParamSettings_eas_clearsucc");
                    }
                    else
                    {
                        tbResult.Text = LangResouorce.GetText("ParamSettings_eas_setfail");
                    }
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
        private async void btnWriteFile_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                SaveFileDialog saveFileDlg = new SaveFileDialog();
                saveFileDlg.Filter = "CSV files (*.csv)|*.csv";
                saveFileDlg.FilterIndex = 1;
                saveFileDlg.RestoreDirectory = true;

                if (saveFileDlg.ShowDialog() == DialogResult.OK)
                {
                    using (StreamWriter writer = new StreamWriter(saveFileDlg.FileName))
                    {
                        // Write header row with column names
                        string headerRow = "Tag";
                        writer.WriteLine(headerRow);

                        // Write data "";
                        string strTag = "";

                        foreach (WhiteInfo item in listWhite.Items)
                        {
                            writer.WriteLine(item.tag);
                        }
                    }
                    System.Windows.MessageBox.Show("数据已保存到 " + saveFileDlg.FileName);
                }

            }
            catch (Exception ex)
            {
                // 处理异常
                Console.WriteLine("发生异常：" + ex.Message);
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
                string currentTime = DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"); // 设置日期和时间的显示格式
                Console.WriteLine(currentTime);
                string strIp = cbDevIp.Text;

                string strUrl = "http://" + strIp + ":8080/moduleapi/eascfg";

                string jsonData = "{\"method\":\"readtag\"}";
                Task<string> returnstr = PostJson(strUrl, jsonData);
                string result = await returnstr;
                if (result == string.Empty)
                {
                    tbResult.Text = LangResouorce.GetText("ParamSettings_eas_getfail");
                    return;
                }

                WhitePacket varData = JsonConvert.DeserializeObject<WhitePacket>(result);

                int iResult = varData.err_code;
                if (iResult == 0)
                {
                    int iCount = varData.tagcount;
                    List<string> myCode = varData.epc;

                    ObservableCollection<WhiteInfo> items = new ObservableCollection<WhiteInfo>();
                    for (int i = 0; i < iCount; i++)
                    {
                        string strCode = myCode[i];
                        items.Add(new WhiteInfo { no = Convert.ToString(i), tag = strCode });

                    }
                    listWhite.ItemsSource = items;
                    tbResult.Text = LangResouorce.GetText("ParamSettings_eas_getalarmsucc") + iCount;
                }
                else
                {
                    tbResult.Text = LangResouorce.GetText("ParamSettings_eas_getfail");
                }
            }
            catch (Exception)
            {
                // 处理异常的代码
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

                WhitePacket varData = new WhitePacket();

                List<string> myList = new List<string>();
                foreach (WhiteInfo item in listWhite.Items)
                {
                    myList.Add(item.tag);
                }

                varData.epc = myList;
                varData.method = "writetag";
                varData.tagcount = listWhite.Items.Count;
                varData.createtime = currentTime;
                 
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
