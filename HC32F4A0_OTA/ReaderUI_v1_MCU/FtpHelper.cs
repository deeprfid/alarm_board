
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Windows;

namespace ReaderManager
{
    public class FtpHelper
    {
        public static  string path = ""; //目标路径
        private static readonly string username = "RfidDemo"; //ftp用户名
        private static readonly string password = "Silion123"; //ftp密码

        //获取ftp上面的文件和文件夹
        public static string[] GetFileList(string dir)
        {
            string[] downloadFiles;

            StringBuilder result = new StringBuilder();

            FtpWebRequest request;

            try
            {
                request = (FtpWebRequest)FtpWebRequest.Create(new Uri(path + dir));

                request.UseBinary = true;

                request.Credentials = new NetworkCredential(username, password);//设置用户名和密码

                request.Method = WebRequestMethods.Ftp.ListDirectory;

                request.UseBinary = true;

                request.UsePassive = true; //选择主动还是被动模式 , 这句要加上的。

                request.KeepAlive = false;//一定要设置此属性，否则一次性下载多个文件的时候，会出现异常。
                WebResponse response = request.GetResponse();
                StreamReader reader = new StreamReader(response.GetResponseStream());

                string line = reader.ReadLine();

                while (line != null)

                {

                    result.Append(line);

                    result.Append("\n");

                    line = reader.ReadLine();

                }

                result.Remove(result.ToString().LastIndexOf('\n'), 1);

                reader.Close();

                response.Close();

                return result.ToString().Split('\n');
            }
            catch (Exception)
            {
                downloadFiles = null;
                return downloadFiles;
            }
        }

        /// <summary>
        /// 从ftp服务器上获取文件并将内容全部转换成string返回
        /// </summary>
        /// <param name="fileName">文件名称</param>
        /// <param name="dir">路径</param>
        /// <returns></returns>
        public static string GetFileStr(string fileName, string dir)
        {
            FtpWebRequest reqFTP;
            try
            {
                reqFTP = (FtpWebRequest)FtpWebRequest.Create(new Uri(path + dir + "/" + fileName));
                reqFTP.Method = WebRequestMethods.Ftp.DownloadFile;
                reqFTP.UseBinary = true;
                reqFTP.Credentials = new NetworkCredential(username, password);
                reqFTP.UsePassive = true; //选择主动还是被动模式 , 这句要加上的。
                reqFTP.KeepAlive = false;//一定要设置此属性，否则一次性下载多个文件的时候，会出现异常。
                FtpWebResponse response = (FtpWebResponse)reqFTP.GetResponse();

                Stream ftpStream = response.GetResponseStream();

                StreamReader reader = new StreamReader(ftpStream);

                string fileStr = reader.ReadToEnd();

                reader.Close();
                ftpStream.Close();
                response.Close();
                return fileStr;
            }
            catch (Exception)
            {
                return null;
            }
        }

        public static FtpFileInfo[] GetFtpFileInfos(string ftpPath,string fileName)
        {
            LinkedList<FtpFileInfo> linkedList = new LinkedList<FtpFileInfo>();
            try
            {
                var url = "";
                if (ftpPath == "")
                    url = path +"/"+ fileName;
                 else
                    url = ftpPath + "/" + fileName;

                var reqFtp = (FtpWebRequest)FtpWebRequest.Create(new Uri(url));
                reqFtp.UsePassive = true;
                reqFtp.UseBinary = true;
                reqFtp.Credentials = new NetworkCredential(username, password);
                reqFtp.Method = WebRequestMethods.Ftp.ListDirectoryDetails;
                reqFtp.Timeout = 1000;
                var response = (FtpWebResponse)reqFtp.GetResponse();
                var reader = new StreamReader(response.GetResponseStream(), Encoding.UTF8);
                string fileDetail = reader.ReadLine();
                while (fileDetail != null)
                {
                    linkedList.AddLast(new FtpFileInfo(fileDetail));
                    fileDetail = reader.ReadLine();
                }
                reader.Close();
                response.Close();
                return linkedList.ToArray();
            }
            catch (Exception)
            {
                return linkedList.ToArray();
            }
        }
        

        /// <summary>
        /// 从ftp下载文件到本地文件夹
        /// </summary>
        /// <param name="fullSourcePath">ftp文件完整地址>/param>
        /// <param name="targetDirectoryPath">目标文件夹路径>/param>
        /// <param name="username">用户名>/param>
        /// <param name="password">密码>/param>
        public static void DownLoadFile(string fileName, string targetDirectoryPath)
        {
            Uri uri = new Uri(path + "/" + fileName);
            string directory = System.IO.Path.GetFullPath(targetDirectoryPath);
            string filename = System.IO.Path.GetFileName(uri.LocalPath);
            string FileName = System.IO.Path.GetFullPath(directory) + System.IO.Path.DirectorySeparatorChar.ToString() + System.IO.Path.GetFileName(filename); //创建一个文件流            
            FileStream fs = null;
            Stream responseStream = null;
            try
            {
                //创建一个与FTP服务器联系的FtpWebRequest对象                
                FtpWebRequest request = (FtpWebRequest)WebRequest.Create(uri);
                request.UsePassive = true; //选择主动还是被动模式 , 这句要加上的。
                request.KeepAlive = false;//一定要设置此属性，否则一次性下载多个文件的时候，会出现异常。
                //设置请求的方法是FTP文件下载                
                request.Method = WebRequestMethods.Ftp.DownloadFile;
                //连接登录FTP服务器                
                request.Credentials = new NetworkCredential(username, password);
                //获取一个请求响应对象                
                FtpWebResponse response = (FtpWebResponse)request.GetResponse();
                //获取请求的响应流                
                responseStream = response.GetResponseStream();
                //判断本地文件是否存在，如果存在，删除             
                if (File.Exists(FileName))
                {
                    File.Delete(FileName);
                }
                if (!Directory.Exists(directory))
                {
                    Directory.CreateDirectory(directory);
                }
                fs = File.Create(FileName);
                if (fs != null)
                {
                    int buffer_count = 65536;
                    byte[] buffer = new byte[buffer_count];
                    int size = 0;
                    while ((size = responseStream.Read(buffer, 0, buffer_count)) > 0)
                    {
                        fs.Write(buffer, 0, size);
                    }
                    fs.Flush();
                    fs.Close();
                    responseStream.Close();
                }
            }
            finally
            {
                if (fs != null)
                    fs.Close();
                if (responseStream != null)
                    responseStream.Close();
            }
        }

        public static List<string> DownLoadDirectory(string fileName, string targetDirectoryPath)
        {
            List<string> fileNames = new List<string>();
            try
            {
                var ftpFileInfos = GetFtpFileInfos("", fileName);
                if (ftpFileInfos != null && ftpFileInfos.Count() > 0)
                {
                    foreach (var file in ftpFileInfos)
                    {
                        string fullSourcePath = "/" + file.FileName;
                        string fullTargetPath = "/" + file.FileName;
                        if (file.UnixFileType == "d")
                        {
                            if (!string.IsNullOrEmpty(file.FileName.Replace(".", "")))
                            {
                                DownLoadDirectory(fileName + "/" + fullSourcePath, fullTargetPath);
                            }
                        }
                        else
                        {
                            DownLoadFile(fileName + fullSourcePath, targetDirectoryPath);
                            fileNames.Add(file.FileName);
                        }
                    }
                }
                return fileNames;
            }
            catch
            {
                return null;
            }
        }

        public static void app_UpdateFinish(string path, string FinalZipName)
        {
            var url = path + "\\download";

            //解压下载后的文件
            if (Directory.Exists(url))
            {
                //后改的 先解压滤波zip植入ini然后再重新压缩
                string dirEcgPath = path + "\\download";
                //开始解压压缩包
                try
                {
                    //复制新文件替换旧文件
                    DirectoryInfo TheFolder = new DirectoryInfo(dirEcgPath);
                    foreach (FileInfo NextFile in TheFolder.GetFiles())
                    {
                        File.Copy(NextFile.FullName, path + NextFile.Name, true);
                        File.Delete(NextFile.FullName);
                    }
                    //Directory.Delete(dirEcgPath, true);
                    //File.Delete(path);
                    //覆盖完成 重新启动程序
                    //path = Application.StartupPath + "\\program";
                    System.Diagnostics.Process process = new System.Diagnostics.Process();
                    process.StartInfo.FileName = "ModuleReaderManager.exe";
                    process.StartInfo.WorkingDirectory = path;//要掉用得exe路径例如:"C:\windows";               
                    process.StartInfo.CreateNoWindow = true;
                    process.Start();


                }
                catch (Exception)
                {

                }
            }
        }

        public static bool IsInternetAvailable()
        {
            try
            {
                Dns.GetHostEntry("www.google.com"); //using System.Net;
                return true;
            }
            catch (SocketException)
            {
                return false;
            }
        }

    }

    public class FtpFileInfo
    {
        public string UnixFileType { get; set; }
        public string Permission { get; set; }
        public string NumberOfHardLinks { get; set; }
        public string Owner { get; set; }
        public string Group { get; set; }
        public string Size { get; set; }
        public string LastModifiedDate { get; set; }
        public string FileName { get; set; }
        public string FileDetail { get; set; }

        public FtpFileInfo(string fileDetail)
        {
            this.FileDetail = fileDetail;
            int counter = 1;
            string[] propertyBlocks = fileDetail.Split(' ');
            foreach (string propertyBlock in propertyBlocks)
            {
                switch (counter)
                {
                    case 1:
                        {
                            //unix file types &amp; permissions
                            if (string.IsNullOrEmpty(propertyBlock))
                            {
                                continue;
                            }
                            else
                            {
                                if (propertyBlock.Length == 10)
                                {
                                    UnixFileType = propertyBlock[0].ToString();
                                    Permission = propertyBlock.Substring(1);
                                }
                                counter++;
                            }
                        }
                        break;
                    case 2:
                        {
                            //number of hard links
                            if (string.IsNullOrEmpty(propertyBlock))
                            {
                                continue;
                            }
                            else
                            {
                                NumberOfHardLinks = propertyBlock;
                                counter++;
                            }
                        }
                        break;
                    case 3:
                        {
                            //owner
                            if (string.IsNullOrEmpty(propertyBlock))
                            {
                                continue;
                            }
                            else
                            {
                                Owner = propertyBlock;
                                counter++;
                            }
                        }
                        break;
                    case 4:
                        {
                            //group
                            if (string.IsNullOrEmpty(propertyBlock))
                            {
                                continue;
                            }
                            else
                            {
                                Group = propertyBlock;
                                counter++;
                            }
                        }
                        break;
                    case 5:
                        {
                            //size
                            if (string.IsNullOrEmpty(propertyBlock))
                            {
                                continue;
                            }
                            else
                            {
                                Size = propertyBlock;
                                counter++;
                            }
                        }
                        break;
                    case 6:
                    case 7:
                    case 8:
                        {
                            //last-modified date
                            if (string.IsNullOrEmpty(propertyBlock))
                            {
                                continue;
                            }
                            else
                            {
                                LastModifiedDate += propertyBlock + " ";
                                counter++;
                            }
                        }
                        break;
                    case 9:
                        {
                            //file name
                            if (string.IsNullOrEmpty(propertyBlock))
                            {
                                FileName += " ";
                            }
                            else
                            {
                                FileName += propertyBlock;
                            }
                        }
                        break;
                }
            }
            LastModifiedDate = LastModifiedDate.Trim();
            FileName = FileName.Trim();
        }
    }

}
