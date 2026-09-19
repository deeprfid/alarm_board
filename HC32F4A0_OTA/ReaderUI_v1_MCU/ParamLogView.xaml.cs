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
using System.Timers;
using System.Windows.Forms;
using System.ComponentModel;
using ClosedXML.Excel;

namespace ReaderManager
{
    /// <summary>
    /// ParamLogView.xaml 的交互逻辑
    /// </summary>
    public partial class ParamLogView : System.Windows.Controls.UserControl
    {
        ReaderParamsViewModel rpViewModel = null;
        MainWindow rootWnd;
        private System.Windows.Forms.Timer myTime;

        public ParamLogView()
        {
            InitializeComponent();

            rpViewModel = ReaderParamsViewModel.GetReaderParamsViewModel();
            rootWnd = ReaderParamsViewModel.GetMainWindow();
            rootWnd.AllViews["ParamLogView"] = this;

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
            tbStart.Text = currentTime;
            tbEnd.Text = currentTime;
            Console.WriteLine(currentTime);

            return 0;
        }

        public int GetParams(bool isTip = false)
        {
            return 0;
        }


        private async void btnClear_Click(object sender, RoutedEventArgs e)
        {
            listAlarm.ItemsSource = null;
            listAlarm.Items.Clear();

            try
            {
                string strIp = cbDevIp.Text;

                string strUrl = "http://" + strIp + ":8080/moduleapi/eascfg";

                string jsonData = "{\"method\":\"dellog\"}";
                Task<string> returnstr = PostJson(strUrl, jsonData);
                string result = await returnstr;
                if (result == string.Empty)
                {
                    Console.WriteLine(LangResouorce.GetText("ParamSettings_eas_sgetfail"));
                    return;
                }

                AlarmData varData = JsonConvert.DeserializeObject<AlarmData>(result);
                int iCount = varData.tagcount;
                if (varData.msg == "success")
                {
                    Console.WriteLine(LangResouorce.GetText("ParamSettings_eas_setsucc"));
                }
                else
                {
                    Console.WriteLine(LangResouorce.GetText("ParamSettings_eas_setfail"));
                }
            }
            catch (Exception)
            {

            }

        }
        private void btnWriteFile_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                SaveFileDialog saveFileDlg = new SaveFileDialog();
                saveFileDlg.Filter = "Excel Files (*.xlsx)|*.xlsx"; // 修改文件过滤器
                saveFileDlg.FilterIndex = 1;
                saveFileDlg.RestoreDirectory = true;

                if (saveFileDlg.ShowDialog() == DialogResult.OK)
                {
                    // 创建 Excel 文件
                    string filePath = saveFileDlg.FileName;
                    using (var workbook = new XLWorkbook())
                    {
                        // 添加工作表
                        var worksheet = workbook.Worksheets.Add("Alarm Data");

                        // 写入标题行
                        worksheet.Cell(1, 1).Value = "EPC";
                        worksheet.Cell(1, 2).Value = "通道";
                        worksheet.Cell(1, 3).Value = "时间";

                        int row = 2; // 从第二行开始
                        foreach (Alarm item in listAlarm.Items)
                        {
                            worksheet.Cell(row, 1).Value = item.epc;
                            worksheet.Cell(row, 2).Value = item.deviceNo;
                            worksheet.Cell(row, 3).Value = item.time;
                            row++;
                        }

                        // 自动调整列宽（可选）
                        worksheet.Columns().AdjustToContents();

                        // 保存文件
                        workbook.SaveAs(filePath);
                    }

                    System.Windows.MessageBox.Show("数据已保存到 " + saveFileDlg.FileName);
                }
            }
            catch (Exception ex)
            {
                // 处理异常
                System.Windows.MessageBox.Show("保存失败: " + ex.Message);
            }

            /*      try
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
                              string headerRow = "Infomaton";
                              writer.WriteLine(headerRow);

                              // Write data "";
                              string strTag = "";

                              foreach (Alarm item in listAlarm.Items)
                              {
                                  writer.WriteLine(item.epc + " " + item.deviceNo + " " + item.time);
                              }
                          }
                          System.Windows.MessageBox.Show("数据已保存到 " + saveFileDlg.FileName);
                      }

                  }
                  catch (Exception ex)
                  {
                      // 处理异常
                      Console.WriteLine("发生异常：" + ex.Message);
                  }*/

        }
        private async void btnGet_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                string startTime = tbStart.Text;
                string endTime = tbEnd.Text;

                string strIp = cbDevIp.Text;

                string strUrl = "http://" + strIp + ":8080/moduleapi/eascfg";

                string jsonData = "{\"method\":\"readlog\",\"start\":\"" + startTime + "\",\"end\":\"" + endTime + "\"}";
                Task<string> returnstr = PostJson(strUrl, jsonData);
                string result = await returnstr;
                if (result == string.Empty)
                {
                    Console.WriteLine(LangResouorce.GetText("ParamSettings_eas_getfail"));
                    return;
                }

                AlarmData varData = JsonConvert.DeserializeObject<AlarmData>(result);
                int iCount = varData.tagcount;
                if (varData.msg == "success")
                {
                    List<Alarm> myAlarmList = varData.data;
                    ObservableCollection<Alarm> items = new ObservableCollection<Alarm>();

                    for (int i = 0; i < iCount; i++)
                    {
                        Alarm myAlarm = myAlarmList[i];
                        items.Add(new Alarm { deviceNo = myAlarm.deviceNo, time = myAlarm.time, epc =  myAlarm.epc, mem = myAlarm.mem });
                    }
                    listAlarm.ItemsSource = items;
                    Console.WriteLine(LangResouorce.GetText("ParamSettings_eas_getsucc"));
                }
                else
                {
                    Console.WriteLine(LangResouorce.GetText("ParamSettings_eas_getfail"));
                }
            }
            catch (Exception)
            {

            }
     

        }

        bool HasGotParams = false;
        public void ResetView()
        {
            HasGotParams = false;
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
