using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ReaderManager.Models
{

    public class JsonModel
    {
        public static T[] ConvertListToArray<T>(List<T> list)
        {
            if (list != null && list.Count > 0)
            {
                return list.ToArray();
            }
            return null;
        }
        public static T ConvertByteToJonsModel<T>(byte[] bytes, out T json)
        {
            try
            {
                string jsonString = System.Text.Encoding.UTF8.GetString(bytes);
                json = JsonConvert.DeserializeObject<T>(jsonString);
                return json;
            }
            catch (Exception)
            {
                throw;
            }
        }
        public static byte[] ConvertModelToJsonBytes<T>(T model)
        {
            string jsonString = JsonConvert.SerializeObject(model).ToLower();
            byte[] jsonBytes = Encoding.UTF8.GetBytes(jsonString);
            return jsonBytes;
        }
    }

    #region 主动模式Model
    public class ActiveModeJson : INotifyPropertyChanged
    {
        [JsonIgnore]
        public string workPattern { get; set; }

        public string ip;

        private int? tcpOrHttpOrMqtt;
        [JsonIgnore]
        public int? TcpOrHttpOrMqtt
        {
            get
            {
                if (tcpOrHttpOrMqtt != null)
                    return tcpOrHttpOrMqtt;

                if (Upload == null)
                    return null;
                else if (Upload.Tcp != null)
                    return 1;
                else if (Upload.Http != null)
                    return 2;
                else if (Upload.Mqtt != null)
                    return 3;
                else
                    return null;
            }
            set
            {
                if (tcpOrHttpOrMqtt != value)
                {
                    tcpOrHttpOrMqtt = value;
                    IsHttp = IsHttp;
                    IsTcp = IsTcp;
                    IsMqtt = IsMqtt;
                    OnPropertyChanged(nameof(TcpOrHttpOrMqtt));
                }
            }
        }

        private bool isTcpOrHttpOrMqtt;
        [JsonIgnore]
        public bool IsTcpOrHttpOrMqtt
        {
            get
            {
                if (Upload != null && (Upload.Hw_inf == 1 || Upload.Hw_inf == 4 || Upload.Hw_inf == 5))
                    return true;
                else if (Upload != null && Upload.Hw_inf == 2) {
                    TcpOrHttpOrMqtt = 0;
                    return false;
                }
                else if (Upload != null && Upload.Hw_inf == 6)
                    return false;
                else if (TcpOrHttpOrMqtt == null)
                    return false;
                else
                    return TcpOrHttpOrMqtt >= 0;
            }
            set
            {
                if (isTcpOrHttpOrMqtt != value)
                {
                    isTcpOrHttpOrMqtt = value;
                    OnPropertyChanged(nameof(IsTcpOrHttpOrMqtt));
                }
            }
        }

        private bool isTcp;
        [JsonIgnore]
        public bool IsTcp
        {
            get
            {
                if (Upload == null)
                    return false;
                else
                    return TcpOrHttpOrMqtt == 1;
            }
            set
            {
                if (IsTcp && this.Upload.Tcp == null)
                {
                    this.Upload.Http = null;
                    this.Upload.Mqtt = null;
                    this.Upload.Tcp = new tpcModel();
                    this.Upload.Wiegand = null;
                }
                if (isTcp != value)
                {
                    isTcp = value;
                    OnPropertyChanged(nameof(IsTcp));
                }
            }
        }

        private bool isHttp;
        [JsonIgnore]
        public bool IsHttp
        {
            get
            {
                if (Upload == null)
                    return false;
                else
                    return TcpOrHttpOrMqtt == 2;
            }
            set
            {
                if (IsHttp && this.Upload.Http == null)
                {
                    this.Upload.Http = new httpModel();
                    this.Upload.Mqtt = null;
                    this.Upload.Tcp = null;
                    this.Upload.Wiegand = null;
                }

                if (isHttp != value)
                {
                    isHttp = value;
                    OnPropertyChanged(nameof(IsHttp));
                }
            }
        }

        private bool isMqtt;
        [JsonIgnore]
        public bool IsMqtt
        {
            get
            {
                if (Upload == null)
                    return false;
                else
                    return TcpOrHttpOrMqtt == 3;
            }
            set
            {
                if (IsMqtt && this.Upload.Mqtt == null)
                {
                    this.Upload.Http = null;
                    this.Upload.Mqtt = new mqttModel();
                    this.Upload.Tcp = null;
                    this.Upload.Wiegand = null;
                }
                if (isMqtt != value)
                {
                    isMqtt = value;
                    OnPropertyChanged(nameof(IsMqtt));
                }
            }
        }


        private bool isWiegand;
        [JsonIgnore]
        public bool IsWiegand
        {
            get
            {
                if (Upload == null)
                    return false;
                else
                    return Upload != null && (Upload.Hw_inf == 6);
            }
            set
            {
                if (IsWiegand && this.Upload.Wiegand == null)
                {
                    this.Upload.Http = null;
                    this.Upload.Mqtt = null;
                    this.Upload.Tcp = null;
                    this.Upload.Wiegand = new wiegandModel();
                }

                if (isWiegand != value)
                {
                    isWiegand = value;
                    OnPropertyChanged(nameof(IsWiegand));
                }
            }
        }

        private globParamsModel glob_params;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        public globParamsModel Glob_params
        {
            get { return glob_params; }
            set
            {
                if (glob_params != value)
                {
                    if (glob_params != null)
                        glob_params.PropertyChanged -= Model_PropertyChanged;

                    glob_params = value;

                    if (glob_params != null)
                        glob_params.PropertyChanged += Model_PropertyChanged;

                    OnPropertyChanged(nameof(Glob_params));
                }
            }
        }

        private uploadModel upload;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        public uploadModel Upload
        {
            get { return upload; }
            set
            {
                if (upload != value)
                {
                    if (upload != null)
                        upload.PropertyChanged -= Model_PropertyChanged;

                    upload = value;

                    if (upload != null)
                        upload.PropertyChanged += Model_PropertyChanged;

                    OnPropertyChanged(nameof(Upload));
                }
            }
        }

        private bool isGpi_trigger;
        [JsonIgnore]
        public bool IsGpi_trigger
        {
            get
            {
                return isGpi_trigger;
            }
            set
            {
                if (isGpi_trigger != value)
                {
                    isGpi_trigger = value;
                    if (Gpi_trigger == null)
                        Gpi_trigger = value ? new gpi_TriggerModel() : null;

                    OnPropertyChanged(nameof(IsGpi_trigger));
                }
            }
        }

        private gpi_TriggerModel gpi_trigger;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        public gpi_TriggerModel Gpi_trigger
        {
            get { return gpi_trigger; }
            set
            {
                if (gpi_trigger != value)
                {
                    if (gpi_trigger != null)
                        gpi_trigger.PropertyChanged -= Model_PropertyChanged;

                    gpi_trigger = value;

                    if (gpi_trigger != null)
                        gpi_trigger.PropertyChanged += Model_PropertyChanged;

                    OnPropertyChanged(nameof(Gpi_trigger));
                }
            }
        }

        private gpo_ActModel[] gpo_act;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        public gpo_ActModel[] Gpo_act
        {
            get { return gpo_act; }
            set
            {
                if (gpo_act != value)
                {
                    //if (gpo_act != null)
                    //    gpo_act.PropertyChanged -= Model_PropertyChanged;

                    gpo_act = value;

                    //if (gpo_act != null)
                    //    gpo_act.PropertyChanged += Model_PropertyChanged;

                    OnPropertyChanged(nameof(Gpo_act));
                }
            }
        }

        private int[] tag_json_format;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        /// <summary>
        /// 描述通过http和mqtt上传标签数据时，标签的json格式参数，数组中的每个数字代表了标签的一项元数据，具体含义如下：
        /// 0：附加数据（嵌入在盘点操作中读到的某个bank的数据）
        /// 1：天线编号
        /// 2：读到的次数
        /// 3：频点信息
        /// 4：标签空中接口协议编号
        /// 5：RSSI
        /// 6：保留字段
        /// 7：首次读到的时间戳
        /// 8：末次读到的时间戳
        /// </summary>
        public int[] Tag_json_format
        {
            get { return tag_json_format; }
            set
            {
                if (tag_json_format != value)
                {
                    tag_json_format = value;
                    OnPropertyChanged(nameof(Tag_json_format));
                }
            }
        }

        private int[] events;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        /// <summary>
        /// 描述客户端订阅的事件，数组中的每个数字表示一个订阅的事件
        /// 默认值为订阅了心跳和标签数据事件，具体含义如下：
        /// 1：标签数据
        /// 2：心跳
        /// 3：gpi状态改变
        /// 4：空数据（在一个数据整理周期内未发现标签）
        /// 5：标签进入
        /// 6：时间同步请求
        /// </summary>
        public int[] Events
        {
            get { return events; }
            set
            {
                if (events != value)
                {
                    events = value;
                    OnPropertyChanged(nameof(Events));
                }
            }
        }

        private string cus_param;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        /// <summary>
        /// 预留的定制化参数 默认值为空
        /// </summary>
        public string Cus_param
        {
            get
            {
                if (cus_param == null)
                    return "";
                else
                    return cus_param;
            }
            set
            {
                if (cus_param != value)
                {
                    cus_param = value;
                    OnPropertyChanged(nameof(Cus_param));
                }
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;

        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }

        public void ToggleValue(int value)
        {
            if (events.Contains(value))
            {
                events = events.Where(x => x != value).ToArray();
            }
            else
            {
                var newList = new List<int>(events);
                newList.Add(value);
                events = newList.ToArray();
            }
            OnPropertyChanged(nameof(Events));
        }

        private void Model_PropertyChanged(object sender, PropertyChangedEventArgs e)
        {
            // 当嵌套对象的属性更改时，触发 PropertyChanged 事件
            OnPropertyChanged(nameof(e.PropertyName));
        }
    }

    public class app_initModel : INotifyPropertyChanged
    {
        private int usb_type;
        /// <summary>
        /// 读写器usb口初始化类型，合法只为1-2
        /// 1：读写器的usb口初始化为‘HID设备+键盘’
        /// 2：读写器的usb口初始化为‘HID设备+CDC串口’
        /// 默认值为2
        /// </summary>
        public int Usb_type
        {
            get { return usb_type; }
            set
            {
                if (usb_type != value)
                {
                    usb_type = value;
                    OnPropertyChanged(nameof(Usb_type));
                }
            }
        }

        private int max_tb_rec_len;
        /// <summary>
        /// 标签EPC+嵌入盘点读bank数据的最大长度，单位为字节，此值越小则读写器能存储的标签记录时越多。如果应用中要读取的EPC+bank数据大于此值，则读写器会丢弃这条标签数据默认值为62
        /// </summary>
        public int Max_tb_rec_len
        {
            get { return max_tb_rec_len; }
            set
            {
                if (max_tb_rec_len != value)
                {
                    max_tb_rec_len = value;
                    OnPropertyChanged(nameof(Max_tb_rec_len));
                }
            }
        }

        private int evt_que_len;
        /// <summary>
        /// 事件队列长度，一般不修改此成员，默认为60
        /// </summary>
        public int Evt_que_len
        {
            get { return evt_que_len; }
            set
            {
                if (evt_que_len != value)
                {
                    evt_que_len = value;
                    OnPropertyChanged(nameof(Evt_que_len));
                }
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;

        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }

    public class globParamsModel : INotifyPropertyChanged
    {

        private int hb_cylce;
        /// <summary>
        /// 心跳报文发送周期，如果未订阅心跳事件，则此字段无意义默认值为10
        /// </summary>
        public int Hb_cylce
        {
            get { return hb_cylce; }
            set
            {
                if (hb_cylce != value)
                {
                    hb_cylce = value;
                    OnPropertyChanged(nameof(Hb_cylce));
                }
            }
        }


        private int s_buf_size;
        /// <summary>
        /// 发送缓存区长度，有效值为1500-8192单位字节，默认值为1580，此成员的值越大，则单次报文可传输的标签数越多
        /// </summary>
        public int S_buf_size
        {
            get { return s_buf_size; }
            set
            {
                if (s_buf_size != value)
                {
                    s_buf_size = value;
                    OnPropertyChanged(nameof(S_buf_size));
                }
            }
        }


        private string name;
        /// <summary>
        /// 读写器名字  默认值为读写器mac地址
        /// </summary>
        public string Name
        {
            get { return name; }
            set
            {
                if (name != value)
                {
                    name = value;
                    OnPropertyChanged(nameof(Name));
                }
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;

        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }

    public class uploadModel : INotifyPropertyChanged
    {
        private int client_ack;
        /// <summary>
        /// tcp上传模式和串口上传模式下，是否对标签数据报文发确认报文，有效值为0-1
        /// 0：不发确认报文
        /// 1：发确认报文
        /// 默认值为0
        /// </summary>
        public int Client_ack
        {
            get { return client_ack; }
            set
            {
                if (client_ack != value)
                {
                    client_ack = value;
                    OnPropertyChanged(nameof(Client_ack));
                }
            }
        }


        private int crc_enable;
        /// <summary>
        /// tcp上传模式，报文末尾是否添加crc校验
        /// 0：不加crc
        /// 1：加crc
        /// 默认值为1
        /// </summary>
        public int Crc_enable
        {
            get { return crc_enable; }
            set
            {
                if (crc_enable != value)
                {
                    crc_enable = value;
                    OnPropertyChanged(nameof(Crc_enable));
                }
            }
        }


        private int recv_timeout;
        /// <summary>
        /// 当需要接收服务器端的回复时，最长的等待超时时间 默认值为10
        /// </summary>
        public int Recv_timeout
        {
            get { return recv_timeout; }
            set
            {
                if (recv_timeout != value)
                {
                    recv_timeout = value;
                    OnPropertyChanged(nameof(Recv_timeout));
                }
            }
        }


        private int clr_r_buf_time;
        /// <summary>
        /// 在重新建立与服务器连接后可能会收到残留的应答数据，读写器会等待接收一段时间，然后把残留数据清空，此成员为等待时间 默认值为8
        /// </summary>
        public int Clr_r_buf_time
        {
            get { return clr_r_buf_time; }
            set
            {
                if (clr_r_buf_time != value)
                {
                    clr_r_buf_time = value;
                    OnPropertyChanged(nameof(Clr_r_buf_time));
                }
            }
        }

        private int? hw_inf;
        /// <summary>
        /// 上传的硬件接口，有效值为1-6
        /// 1：以太网（配置中必须包含tcp、http或mqtt三对象之一）
        /// 2：串口
        /// 3：hid键盘
        /// 4：4G（配置中必须包含tcp、http或mqtt三对象之一）
        /// 5：无线网（配置中必须包含tcp、http或mqtt三对象之一）
        /// 6：韦根（配置中必须包含wiegand对象）
        /// 默认值为0，表示未选择硬件接口
        /// </summary>
        public int? Hw_inf
        {
            get { return hw_inf; }
            set
            {
                if (hw_inf != value)
                {
                    hw_inf = value;

                    OnPropertyChanged(nameof(Hw_inf));
                }
            }
        }

        private upload_DataAggrModel data_aggr;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        /// <summary>
        /// 上传的硬件接口，有效值为1-6
        /// 1：以太网（配置中必须包含tcp、http或mqtt三对象之一）
        /// 2：串口
        /// 3：hid键盘
        /// 4：4G（配置中必须包含tcp、http或mqtt三对象之一）
        /// 5：无线网（配置中必须包含tcp、http或mqtt三对象之一）
        /// 6：韦根（配置中必须包含wiegand对象）
        /// 默认值为0，表示未选择硬件接口
        /// </summary>
        public upload_DataAggrModel Data_aggr
        {
            get { return data_aggr; }
            set
            {
                if (data_aggr != value)
                {
                    if (data_aggr != null)
                        data_aggr.PropertyChanged -= uploadModel_PropertyChanged;

                    data_aggr = value;

                    if (data_aggr != null)
                        data_aggr.PropertyChanged += uploadModel_PropertyChanged;

                    OnPropertyChanged(nameof(Data_aggr));
                }
            }
        }


        private tpcModel tcp;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        /// <summary>
        /// Socket tcp 方式上传数据的相关参数，不可和http或mqtt对象同时存在
        /// </summary>
        public tpcModel Tcp
        {
            get { return tcp; }
            set
            {
                if (tcp != value)
                {
                    if (tcp != null)
                        tcp.PropertyChanged -= uploadModel_PropertyChanged;

                    tcp = value;

                    if (tcp != null)
                        tcp.PropertyChanged += uploadModel_PropertyChanged;

                    OnPropertyChanged(nameof(Tcp));
                }
            }
        }

        private httpModel http;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        /// <summary>
        /// http 方式上传数据的相关参数，不可和tcp或mqtt对象同时存在
        /// </summary>
        public httpModel Http
        {
            get { return http; }
            set
            {
                if (http != value)
                {
                    if (http != null)
                        http.PropertyChanged -= uploadModel_PropertyChanged;

                    http = value;

                    if (http != null)
                        http.PropertyChanged += uploadModel_PropertyChanged;

                    OnPropertyChanged(nameof(Http));
                }
            }
        }


        private mqttModel mqtt;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        /// <summary>
        /// mqtt方式上传数据的相关参数，不可和http或tcp对象同时存在
        /// </summary>
        public mqttModel Mqtt
        {
            get { return mqtt; }
            set
            {
                if (mqtt != value)
                {
                    if (mqtt != null)
                        mqtt.PropertyChanged -= uploadModel_PropertyChanged;

                    mqtt = value;

                    if (mqtt != null)
                        mqtt.PropertyChanged += uploadModel_PropertyChanged;

                    OnPropertyChanged(nameof(Mqtt));
                }
            }
        }

        private wiegandModel wiegand;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        /// <summary>
        /// wiegand接口上传数据时的相关参数
        /// </summary>
        public wiegandModel Wiegand
        {
            get { return wiegand; }
            set
            {
                if (wiegand != value)
                {
                    if (wiegand != null)
                        wiegand.PropertyChanged -= uploadModel_PropertyChanged;

                    wiegand = value;

                    if (wiegand != null)
                        wiegand.PropertyChanged += uploadModel_PropertyChanged;

                    OnPropertyChanged(nameof(Wiegand));
                }

                //if (tcp != value)
                //{
                //    if (tcp != null)
                //        tcp.PropertyChanged -= uploadModel_PropertyChanged;

                //    tcp = value;

                //    if (tcp != null)
                //        tcp.PropertyChanged += uploadModel_PropertyChanged;

                //    OnPropertyChanged(nameof(Tcp));
                //}
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;

        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }

        private void uploadModel_PropertyChanged(object sender, PropertyChangedEventArgs e)
        {
            // 当嵌套对象的属性更改时，触发属性更改事件
            OnPropertyChanged(e.PropertyName);
        }
    }


    public class upload_DataAggrModel : INotifyPropertyChanged
    {
        private int mode;
        /// <summary>
        /// 数据整理模式，目前只能为1 默认值为1
        /// </summary>
        public int Mode
        {
            get { return mode; }
            set
            {
                if (mode != value)
                {
                    mode = value;
                    OnPropertyChanged(nameof(Mode));
                }
            }
        }


        private int timeval;
        /// <summary>
        /// 数据整理和上传的周期，有效值为50-86400000，单位ms 默认值为2000
        /// </summary>
        public int Timeval
        {
            get { return timeval; }
            set
            {
                if (timeval != value)
                {
                    timeval = value;
                    OnPropertyChanged(nameof(Timeval));
                }
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;

        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }


    public class tpcModel : INotifyPropertyChanged
    {
        private string ser_ip;
        /// <summary>
        /// 服务器ip地址
        /// </summary>
        public string Ser_ip
        {
            get { return ser_ip; }
            set
            {
                if (ser_ip != value)
                {
                    ser_ip = value;
                    OnPropertyChanged(nameof(Ser_ip));
                }
            }
        }

        private string ser_port;
        /// <summary>
        /// 服务器侦听端口，有效值为1001-65535
        /// </summary>
        public string Ser_port
        {
            get { return ser_port; }
            set
            {
                if (ser_port != value)
                {
                    ser_port = value;
                    OnPropertyChanged(nameof(Ser_port));
                }
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;

        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }


    public class httpModel : INotifyPropertyChanged
    {
        private string url;
        /// <summary>
        /// 上传url，最大长度不能超过256个字符
        /// </summary>
        public string Url
        {
            get { return url; }
            set
            {
                if (url != value)
                {
                    url = value;
                    OnPropertyChanged(nameof(Url));
                }
            }
        }

        private int timeout;
        /// <summary>
        /// 读写器等待http回复的超时时间，单位秒，有效值为5-60 默认值为8
        /// </summary>
        public int Timeout
        {
            get { return timeout; }
            set
            {
                if (timeout != value)
                {
                    timeout = value;
                    OnPropertyChanged(nameof(Timeout));
                }
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;

        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }


    public class mqttModel : INotifyPropertyChanged
    {
        private string host;
        /// <summary>
        /// mqtt broker的ip地址或域名，最大长度不能超过48个字符
        /// </summary>
        public string Host
        {
            get
            {
                if (host == null)
                    return "";
                else
                    return host;
            }
            set
            {
                if (host != value)
                {
                    host = value;
                    OnPropertyChanged(nameof(Host));
                }
            }
        }

        private string port;
        /// <summary>
        /// mqtt broker的绑定端口
        /// </summary>
        public string Port
        {
            get
            {
                if (port == null)
                    return "";
                else
                    return port;
            }
            set
            {
                if (port != value)
                {
                    port = value;
                    OnPropertyChanged(nameof(Port));
                }
            }
        }

        private string user;
        /// <summary>
        /// 用户名，最大长度不能超过32个字符
        /// </summary>
        public string User
        {
            get
            {
                if (user == null)
                    return "";
                else
                    return user;
            }
            set
            {
                if (user != value)
                {
                    user = value;
                    OnPropertyChanged(nameof(User));
                }
            }
        }

        private string pwd;
        /// <summary>
        /// 密码，最大长度不能超过32个字符
        /// </summary>
        public string Pwd
        {
            get
            {
                if (pwd == null)
                    return "";
                else
                    return pwd;
            }
            set
            {
                if (pwd != value)
                {
                    pwd = value;
                    OnPropertyChanged(nameof(Pwd));
                }
            }
        }

        private int kal_time;
        /// <summary>
        /// keepalive周期，单位秒，有效值为5-86400
        /// </summary>
        public int Kal_time
        {
            get
            {
                if (kal_time == 0)
                    return 60;
                else
                    return kal_time;
            }
            set
            {
                if (kal_time != value)
                {
                    kal_time = value;
                    OnPropertyChanged(nameof(Kal_time));
                }
            }
        }

        private string pub_topic;
        /// <summary>
        /// 发布主题，最大长度不能超过48个字符
        /// </summary>
        public string Pub_topic
        {
            get
            {
                if (pub_topic == null)
                    return "";
                else
                    return pub_topic;
            }
            set
            {
                if (pub_topic != value)
                {
                    pub_topic = value;
                    OnPropertyChanged(nameof(Pub_topic));
                }
            }
        }


        private int pub_qos;
        /// <summary>
        /// 发布消息的qos等级，有效值为0-2
        /// </summary>
        public int Pub_qos
        {
            get
            {
                //下拉框数值是0~3，第0个是"请选择"，这里需要去除
                if (pub_qos > 0)
                    return pub_qos - 1;
                else
                    return pub_qos;
            }
            set
            {
                if (pub_qos != value)
                {
                    pub_qos = value;
                    OnPropertyChanged(nameof(Pub_qos));
                }
            }
        }

        private string sub_b_topic;
        /// <summary>
        /// 广播订阅主题，最大长度不能超过48个字符
        /// </summary>
        public string Sub_b_topic
        {
            get
            {
                if (sub_b_topic == null)
                    return "";
                else
                    return sub_b_topic;
            }
            set
            {
                if (sub_b_topic != value)
                {
                    sub_b_topic = value;
                    OnPropertyChanged(nameof(Sub_b_topic));
                }
            }
        }

        private int sub_b_qos;
        /// <summary>
        /// 广播订阅消息的qos等级，有效值为0-2
        /// </summary>
        public int Sub_b_qos
        {
            get
            {
                //下拉框数值是0~3，第0个是"请选择"，这里需要去除
                if (sub_b_qos > 0)
                    return sub_b_qos - 1;
                else
                    return sub_b_qos;
            }
            set
            {
                if (sub_b_qos != value)
                {
                    sub_b_qos = value;
                    OnPropertyChanged(nameof(Sub_b_qos));
                }
            }
        }

        private string sub_u_topic;
        /// <summary>
        /// 单播订阅主题，最大长度不能超过48个字符
        /// </summary>
        public string Sub_u_topic
        {
            get
            {
                if (sub_u_topic == null)
                    return "";
                else
                    return sub_u_topic;
            }
            set
            {
                if (sub_u_topic != value)
                {
                    sub_u_topic = value;
                    OnPropertyChanged(nameof(Sub_u_topic));
                }
            }
        }

        private int sub_u_qos;
        /// <summary>
        /// 单播订阅消息的qos等级，有效值为0-2
        /// </summary>
        public int Sub_u_qos
        {
            get
            {
                //下拉框数值是0~3，第0个是"请选择"，这里需要去除
                if (sub_u_qos > 0)
                    return sub_u_qos - 1;
                else
                    return sub_u_qos;
            }
            set
            {
                if (sub_u_qos != value)
                {
                    sub_u_qos = value;
                    OnPropertyChanged(nameof(Sub_u_qos));
                }
            }
        }

        private int tls;
        /// <summary>
        /// 是否启用ssl/tls安全连接，有效值为0-1
        /// </summary>
        public int Tls
        {
            get { return tls; }
            set
            {
                if (tls != value)
                {
                    tls = value;
                    OnPropertyChanged(nameof(Tls));
                }
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;

        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }
    public class wiegandModel : INotifyPropertyChanged
    {

        private int pls_width;
        /// <summary>
        /// 脉冲宽度，单位为10us，有效值为4-20
        /// </summary>
        public int Pls_width
        {
            get { return pls_width; }
            set
            {
                if (pls_width != value)
                {
                    pls_width = value;
                    OnPropertyChanged(nameof(Pls_width));
                }
            }
        }


        private int pls_interval;
        /// <summary>
        /// 脉冲间隔，单位为100us，有效值为8-200
        /// </summary>
        public int Pls_interval
        {
            get { return pls_interval; }
            set
            {
                if (pls_interval != value)
                {
                    pls_interval = value;
                    OnPropertyChanged(nameof(Pls_interval));
                }
            }
        }


        private int data_interval;
        /// <summary>
        /// 数据输出间隔，单位为ms，有效值为50-1000
        /// </summary>
        public int Data_interval
        {
            get { return data_interval; }
            set
            {
                if (data_interval != value)
                {
                    data_interval = value;
                    OnPropertyChanged(nameof(Data_interval));
                }
            }
        }


        private int type;
        /// <summary>
        /// 韦根类型，有效值为1-3。1：韦根26, 2：韦根34, 3：韦根66
        /// </summary>
        public int Type
        {
            get { return type; }
            set
            {
                if (type != value)
                {
                    type = value;
                    OnPropertyChanged(nameof(Type));
                }
            }
        }


        private int bytes_order;
        /// <summary>
        /// 数据输出字节顺序，有效值为0-1。0：高字节在前,1：低字节在前
        /// </summary>
        public int Bytes_order
        {
            get { return bytes_order; }
            set
            {
                if (bytes_order != value)
                {
                    bytes_order = value;
                    OnPropertyChanged(nameof(Bytes_order));
                }
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;

        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }

    public class gpi_TriggerModel : INotifyPropertyChanged
    {

        private int mode;
        /// <summary>
        /// 触发模式，有效值为1-4
        /// 1：满足条件1启动盘点，满足条件2停止盘点
        /// 2：满足条件1启动盘点，盘点固定时长停止
        /// 3：满足条件1或条件2启动盘点，盘点固定时长停止
        /// 4：满足条件1或条件2启动盘点，满足另一个条件停止盘点
        /// </summary>
        public int Mode
        {
            get { return mode; }
            set
            {
                if (mode != value)
                {
                    mode = value;
                    OnPropertyChanged(nameof(Mode));
                }
            }
        }

        private int timeval;
        /// <summary>
        /// 当mode为2或3时，此字段为盘点的时长，当mode为1或4时，此字段为上一次的启动条件发生反转并持续的时间（如果达不到反转持续时间则无法启动下一次触发开始），单位为ms
        /// </summary>
        public int Timeval
        {
            get { return timeval; }
            set
            {
                if (timeval != value)
                {
                    timeval = value;
                    OnPropertyChanged(nameof(Timeval));
                }
            }
        }

        /// <summary>
        /// 触发条件1
        /// </summary>
        private condModel[] cond_1;
        public condModel[] Cond_1
        {
            get { return cond_1; }
            set
            {
                if (cond_1 != value)
                {
                    //if (cond_1 != null)
                    //{
                    //    cond_1.PropertyChanged -= Cond_PropertyChanged;
                    //}

                    cond_1 = value;

                    //if (cond_1 != null)
                    //{
                    //    cond_1.PropertyChanged += Cond_PropertyChanged;
                    //}

                    OnPropertyChanged(nameof(Cond_1));
                }
            }
        }

        /// <summary>
        /// 触发条件2
        /// </summary>
        private condModel[] cond_2;
        public condModel[] Cond_2
        {
            get { return cond_2; }
            set
            {
                if (cond_2 != value)
                {
                    //if (cond_2 != null)
                    //{
                    //    cond_2.PropertyChanged -= Cond_PropertyChanged;
                    //}

                    cond_2 = value;
                    //if (cond_2 != null)
                    //{
                    //    cond_2.PropertyChanged += Cond_PropertyChanged;
                    //}

                    OnPropertyChanged(nameof(Cond_2));
                }
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;

        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }

        private void Cond_PropertyChanged(object sender, PropertyChangedEventArgs e)
        {
            // 当嵌套对象 Cond_1 或 Cond_2 的属性更改时，触发 PropertyChanged 事件
            OnPropertyChanged(nameof(Cond_1));
            OnPropertyChanged(nameof(Cond_2));
        }
    }


    public class condModel : INotifyPropertyChanged
    {
        private int id;
        /// <summary>
        /// gpi编号，有效值为1-读写器gpi端口数量，不同读写器型号的gpi端口数量可能不一样
        /// </summary>
        public int Id
        {
            get { return id; }
            set
            {
                if (id != value)
                {
                    id = value;
                    OnPropertyChanged(nameof(Id));
                }
            }
        }

        private int state;
        /// <summary>
        /// gpi状态，有效值为0-1
        /// </summary>
        public int State
        {
            get { return state; }
            set
            {
                if (state != value)
                {
                    state = value;
                    OnPropertyChanged(nameof(State));
                }
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;

        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }

    public class gpo_ActModel : INotifyPropertyChanged
    {

        private int id;
        /// <summary>
        /// gpo编号，有效值为1-读写器gpo端口数量，不同读写器型号的gpo端口数量可能不一样
        /// </summary>
        public int Id
        {
            get { return id; }
            set
            {
                if (id != value)
                {
                    id = value;
                    OnPropertyChanged(nameof(Id));
                }
            }
        }


        private int state;
        /// <summary>
        /// gpo状态，有效值为0-1
        /// </summary>
        public int State
        {
            get { return state; }
            set
            {
                if (state != value)
                {
                    state = value;
                    OnPropertyChanged(nameof(State));
                }
            }
        }

        private int dur;
        /// <summary>
        /// gpo状态持续的时间，单位ms，有效值为5-86400000，当此时间过期后gpo的状态则会恢复到state的相反状态
        /// </summary>
        public int Dur
        {
            get { return dur; }
            set
            {
                if (dur != value)
                {
                    dur = value;
                    OnPropertyChanged(nameof(Dur));
                }
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;

        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }
    #endregion

    #region 静态配置Model

    public class StaticModelJson : INotifyPropertyChanged
    {

        private bool isGpO_enable;
        [JsonIgnore]
        public bool IsGpO_enable
        {
            get
            {
                return isGpO_enable;
            }
            set
            {
                if (isGpO_enable != value)
                {
                    isGpO_enable = value;
                    if (Gpos == null)
                        Gpos = value ? new GposModel[4] : null;

                    OnPropertyChanged(nameof(IsGpO_enable));
                }
            }
        }

        private bool isTagfilter;
        [JsonIgnore]
        public bool IsTagfilter
        {
            get
            {
                return isTagfilter;
            }
            set
            {
                if (isTagfilter != value)
                {
                    isTagfilter = value;
                    if (Tagops_param!= null&& Tagops_param.Tagfilter==null)
                        Tagops_param.Tagfilter = value ? new TagfilterModel() : null;

                    OnPropertyChanged(nameof(IsTagfilter));
                }
            }
        }




        private bool isBankdata;
        [JsonIgnore]
        public bool IsBankdata
        {
            get
            {
                return isBankdata;
            }
            set
            {
                if (isBankdata != value)
                {
                    isBankdata = value;
                    if (Tagops_param != null && Tagops_param.Bankdata == null)
                        Tagops_param.Bankdata = value ? new BankdataModel() : null;

                    OnPropertyChanged(nameof(IsBankdata));
                }
            }
        }

        private bool isBankdata2;
        [JsonIgnore]
        public bool IsBankdata2
        {
            get
            {
                return isBankdata2;
            }
            set
            {
                if (isBankdata2 != value)
                {
                    isBankdata2 = value;
                    if (Tagops_param != null && Tagops_param.Bankdata2 == null)
                        Tagops_param.Bankdata2 = value ? new BankdataModel() : null;

                    OnPropertyChanged(nameof(IsBankdata2));
                }
            }
        }

        private bool isBankdata3;
        [JsonIgnore]
        public bool IsBankdata3
        {
            get
            {
                return isBankdata3;
            }
            set
            {
                if (isBankdata3 != value)
                {
                    isBankdata3 = value;
                    if (Tagops_param != null && Tagops_param.Bankdata3 == null)
                        Tagops_param.Bankdata3 = value ? new BankdataModel() : null;

                    OnPropertyChanged(nameof(IsBankdata3));
                }
            }
        }
        //Bankdata


        //classGposModel

        private GposModel[] gpos;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        public GposModel[] Gpos
        {
            get { return gpos; }
            set
            {
                if (gpos != value)
                {
                    gpos = value;
                    OnPropertyChanged(nameof(GposModel));
                }
            }
        }

        private AppInit app_init;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        public AppInit App_init
        {
            get { return app_init; }
            set
            {
                if (app_init != value)
                {
                    app_init = value;
                    OnPropertyChanged(nameof(App_init));
                }
            }
        }

        private Tagops_paramModel tagops_param;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        public Tagops_paramModel Tagops_param
        {
            get { return tagops_param; }
            set
            {
                if (tagops_param != value)
                {
                    tagops_param = value;
                    OnPropertyChanged(nameof(Tagops_param));
                }
            }
        }

        private ProtocolModel protocol;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        public ProtocolModel Protocol
        {
            get { return protocol; }
            set
            {
                if (protocol != value)
                {
                    protocol = value;
                    OnPropertyChanged(nameof(Protocol));
                }
            }
        }

        private RfParam rf;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        public RfParam Rf
        {
            get { return rf; }
            set
            {
                if (rf != value)
                {
                    rf = value;
                    OnPropertyChanged(nameof(Rf));
                }
            }
        }

        private TagDataModel tag_data;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        public TagDataModel Tag_data
        {
            get { return tag_data; }
            set
            {
                if (tag_data != value)
                {
                    tag_data = value;
                    OnPropertyChanged(nameof(Tag_data));
                }
            }
        }

        private UartModel uart1;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        public UartModel Uart1
        {
            get { return uart1; }
            set
            {
                if (uart1 != value)
                {
                    uart1 = value;
                    OnPropertyChanged(nameof(Uart1));
                }
            }
        }

        private UartModel uart2;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        public UartModel Uart2
        {
            get { return uart2; }
            set
            {
                if (uart2 != value)
                {
                    uart2 = value;
                    OnPropertyChanged(nameof(Uart2));
                }
            }
        }

        private EthernetModel ethernet;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        public EthernetModel Ethernet
        {
            get { return ethernet; }
            set
            {
                if (ethernet != value)
                {
                    ethernet = value;
                    OnPropertyChanged(nameof(Ethernet));
                }
            }
        }

        private WlanModel wlan;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        public WlanModel Wlan
        {
            get { return wlan; }
            set
            {
                if (wlan != value)
                {
                    wlan = value;
                    OnPropertyChanged(nameof(Wlan));
                }
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;

        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }

    public class GposModel : INotifyPropertyChanged
    {
        private int id;
        public int Id
        {
            get { return id; }
            set
            {
                if (id != value)
                {
                    id = value;
                    OnPropertyChanged(nameof(Id));
                }
            }
        }

        private int state;
        public int State
        {
            get { return state; }
            set
            {
                if (state != value)
                {
                    state = value;
                    OnPropertyChanged(nameof(State));
                }
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;

        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }

    }

    public class AppInit : INotifyPropertyChanged
    {
        private int usb_type;
        public int Usb_type
        {
            get { return usb_type; }
            set
            {
                if (usb_type != value)
                {
                    usb_type = value;
                    OnPropertyChanged(nameof(Usb_type));
                }
            }
        }

        private int max_tb_rec_len;
        public int Max_tb_rec_len
        {
            get { return max_tb_rec_len; }
            set
            {
                if (max_tb_rec_len != value)
                {
                    max_tb_rec_len = value;
                    OnPropertyChanged(nameof(Max_tb_rec_len));
                }
            }
        }

        private int evt_que_len;
        public int Evt_que_len
        {
            get { return evt_que_len; }
            set
            {
                if (evt_que_len != value)
                {
                    evt_que_len = value;
                    OnPropertyChanged(nameof(Evt_que_len));
                }
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;

        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }

    public class Tagops_paramModel : INotifyPropertyChanged
    {
        private InventoryModel inventory;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        public InventoryModel Inventory
        {
            get { return inventory; }
            set
            {
                if (inventory != value)
                {
                    inventory = value;
                    OnPropertyChanged(nameof(Inventory));
                }
            }
        }

        private AccessOpModel accessop;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        public AccessOpModel Accessop
        {
            get { return accessop; }
            set
            {
                if (accessop != value)
                {
                    accessop = value;
                    OnPropertyChanged(nameof(Accessop));
                }
            }
        }

        private TagfilterModel tagfilter;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        public TagfilterModel Tagfilter
        {
            get { return tagfilter; }
            set
            {
                if (tagfilter != value)
                {
                    tagfilter = value;
                    OnPropertyChanged(nameof(Tagfilter));
                }
            }
        }

        private BankdataModel bankdata;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        public BankdataModel Bankdata
        {
            get { return bankdata; }
            set
            {
                if (bankdata != value)
                {
                    bankdata = value;
                    OnPropertyChanged(nameof(Bankdata));
                }
            }
        }

        private BankdataModel bankdata2;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        public BankdataModel Bankdata2
        {
            get { return bankdata2; }
            set
            {
                if (bankdata2 != value)
                {
                    bankdata2 = value;
                    OnPropertyChanged(nameof(Bankdata2));
                }
            }
        }

        private BankdataModel bankdata3;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        public BankdataModel Bankdata3
        {
            get { return bankdata3; }
            set
            {
                if (bankdata3 != value)
                {
                    bankdata3 = value;
                    OnPropertyChanged(nameof(Bankdata3));
                }
            }
        }


        public event PropertyChangedEventHandler PropertyChanged;

        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }


    public class InventoryModel : INotifyPropertyChanged
    {
        private int cycle;
        public int Cycle
        {
            get { return cycle; }
            set
            {
                if (cycle != value)
                {
                    cycle = value;
                    OnPropertyChanged(nameof(Cycle));
                }
            }
        }

        private int interval;
        public int Interval
        {
            get { return interval; }
            set
            {
                if (interval != value)
                {
                    interval = value;
                    OnPropertyChanged(nameof(Interval));
                }
            }
        }

        private int inv_mode;
        public int Inv_mode
        {
            get { return inv_mode; }
            set
            {
                if (inv_mode != value)
                {
                    inv_mode = value;
                    OnPropertyChanged(nameof(Inv_mode));
                }
            }
        }

        private int[] ants;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        public int[] Ants
        {
            get { return ants; }
            set
            {
                if (ants != value)
                {
                    ants = value;
                    OnPropertyChanged(nameof(Ants));
                }
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;

        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }

    public class AccessOpModel : INotifyPropertyChanged
    {
        private int ant;
        public int Ant
        {
            get { return ant; }
            set
            {
                if (ant != value)
                {
                    ant = value;
                    OnPropertyChanged(nameof(Ant));
                }
            }
        }

        private int timeout;
        public int Timeout
        {
            get { return timeout; }
            set
            {
                if (timeout != value)
                {
                    timeout = value;
                    OnPropertyChanged(nameof(Timeout));
                }
            }
        }

        private string aespwd;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        public string Aaespwd
        {
            get { return aespwd; }
            set
            {
                if (aespwd != value)
                {
                    aespwd = value;
                    OnPropertyChanged(nameof(Aaespwd));
                }
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;

        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }

    public class TagfilterModel : INotifyPropertyChanged
    {
        private string bank;
        public string Bank
        {
            get { return bank; }
            set
            {
                if (bank != value)
                {
                    bank = value;
                    OnPropertyChanged(nameof(Bank));
                }
            }
        }

        private string start;
        public string Start
        {
            get { return start; }
            set
            {
                if (start != value)
                {
                    start = value;
                    OnPropertyChanged(nameof(Start));
                }
            }
        }

        private string match;
        public string Match
        {
            get { return match; }
            set
            {
                if (match != value)
                {
                    match = value;
                    OnPropertyChanged(nameof(Match));
                }
            }
        }

        private string mask;
        public string Mask
        {
            get { return mask; }
            set
            {
                if (mask != value)
                {
                    mask = value;
                    OnPropertyChanged(nameof(Mask));
                }
            }
        }


        public event PropertyChangedEventHandler PropertyChanged;

        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }

    public class BankdataModel : INotifyPropertyChanged
    {
        private string bank;
        public string Bank
        {
            get { return bank; }
            set
            {
                if (bank != value)
                {
                    bank = value;
                    OnPropertyChanged(nameof(Bank));
                }
            }
        }

        private string start;
        public string Start
        {
            get { return start; }
            set
            {
                if (start != value)
                {
                    start = value;
                    OnPropertyChanged(nameof(Start));
                }
            }
        }

        private string blkcnt;
        public string Blkcnt
        {
            get { return blkcnt; }
            set
            {
                if (blkcnt != value)
                {
                    blkcnt = value;
                    OnPropertyChanged(nameof(Blkcnt));
                }
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;

        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }


    public class ProtocolModel : INotifyPropertyChanged
    {

        private Gen2Model gen2;
        public Gen2Model Gen2
        {
            get { return gen2; }
            set
            {
                if (gen2 != value)
                {
                    gen2 = value;
                    OnPropertyChanged(nameof(Gen2));
                }
            }
        }
        public event PropertyChangedEventHandler PropertyChanged;

        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }
    public class Gen2Model : INotifyPropertyChanged
    {
        private int session;
        public int Session
        {
            get { return session; }
            set
            {
                if (session != value)
                {
                    session = value;
                    OnPropertyChanged(nameof(Session));
                }
            }
        }

        private int q;
        public int Q
        {
            get { return q; }
            set
            {
                if (q != value)
                {
                    q = value;
                    OnPropertyChanged(nameof(Q));
                }
            }
        }

        private int target;
        public int Target
        {
            get { return target; }
            set
            {
                if (target != value)
                {
                    target = value;
                    OnPropertyChanged(nameof(Target));
                }
            }
        }

        private int profile;
        public int Profile
        {
            get { return profile; }
            set
            {
                if (profile != value)
                {
                    profile = value;
                    OnPropertyChanged(nameof(Profile));
                }
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;

        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }

    public class RfTxPower : INotifyPropertyChanged
    {
        private int id;
        public int Id
        {
            get { return id; }
            set
            {
                if (id != value)
                {
                    id = value;
                    OnPropertyChanged(nameof(Id));
                }
            }
        }

        private int rp;
        public int Rp
        {
            get { return rp; }
            set
            {
                if (rp != value)
                {
                    rp = value;
                    OnPropertyChanged(nameof(Rp));
                }
            }
        }

        private int wp;
        public int Wp
        {
            get { return rp; }
            set
            {
                if (wp != value)
                {
                    wp = value;
                    OnPropertyChanged(nameof(Wp));
                }
            }
        }

        [JsonIgnore]
        public string AntennaName_Rp => "Ant Power" + Id;

        [JsonIgnore]
        public string AntennaName_Wp => "Ant Power" + Id;
        public event PropertyChangedEventHandler PropertyChanged;

        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }

    public class RfParam : INotifyPropertyChanged
    {
        private int ant_max_dwell_time;
        public int Ant_max_dwell_time
        {
            get { return ant_max_dwell_time; }
            set
            {
                if (ant_max_dwell_time != value)
                {
                    ant_max_dwell_time = value;
                    OnPropertyChanged(nameof(Ant_max_dwell_time));
                }
            }
        }

        private int hop_mode;
        public int Hop_mode
        {
            get { return hop_mode; }
            set
            {
                if (hop_mode != value)
                {
                    hop_mode = value;
                    OnPropertyChanged(nameof(Hop_mode));
                }
            }
        }

        private int region;
        public int Region
        {
            get { return region; }
            set
            {
                if (region != value)
                {
                    region = value;
                    OnPropertyChanged(nameof(Region));
                }
            }
        }

        private RfTxPower[] tx_powers;
        public RfTxPower[] Tx_powers
        {
            get { return tx_powers; }
            set
            {
                if (tx_powers != value)
                {
                    tx_powers = value;
                    OnPropertyChanged(nameof(Tx_powers));
                }
            }
        }

        private int[] hop_table;
        [JsonProperty(NullValueHandling = NullValueHandling.Ignore)]
        public int[] Hop_table
        {
            get { return hop_table; }
            set
            {
                if (hop_table != value)
                {
                    hop_table = value;
                    OnPropertyChanged(nameof(Hop_table));
                }
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;

        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }

    public class TagDataModel : INotifyPropertyChanged
    {
        private string unique_by_antenna;
        public string Unique_by_antenna
        {
            get { return unique_by_antenna; }
            set
            {
                string oldValue = value;
                if (oldValue == "TRUE")
                    oldValue = "0";
                else if (oldValue == "FALSE")
                    oldValue = "1";
                else
                    oldValue = "-1";

                if (unique_by_antenna != oldValue)
                {
                    unique_by_antenna = oldValue;
                    OnPropertyChanged(nameof(Unique_by_antenna));
                }
            }
        }

        private string unique_by_bank_data;
        public string Unique_by_bank_data
        {
            get { return unique_by_bank_data; }
            set
            {
                string oldValue = value;
                if (oldValue == "True")
                    oldValue = "0";
                else if (oldValue == "False")
                    oldValue = "1";

                if (unique_by_bank_data != oldValue)
                {
                    unique_by_bank_data = oldValue;
                    OnPropertyChanged(nameof(Unique_by_bank_data));
                }
            }
        }

        private string record_highest_rssi;
        public string Record_highest_rssi
        {
            get { return record_highest_rssi; }
            set
            {
                string oldValue = value;
                if (oldValue == "TRUE")
                    oldValue = "0";
                else if (oldValue == "FALSE")
                    oldValue = "1";
                else
                    oldValue = "-1";
                if (record_highest_rssi != oldValue)
                {
                    record_highest_rssi = oldValue;
                    OnPropertyChanged(nameof(Record_highest_rssi));
                }
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;

        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }

    public class UartModel : INotifyPropertyChanged
    {
        private int type;
        public int Type
        {
            get { return type; }
            set
            {
                if (type != value)
                {
                    type = value;
                    OnPropertyChanged(nameof(Type));
                }
            }
        }

        private int address;
        public int Address
        {
            get { return address; }
            set
            {
                if (address != value)
                {
                    address = value;
                    OnPropertyChanged(nameof(Address));
                }
            }
        }

        private int baud;
        public int Baud
        {
            get { return baud; }
            set
            {
                if (baud != value)
                {
                    baud = value;
                    OnPropertyChanged(nameof(Baud));
                }
            }
        }

        private int data_bits;
        public int Data_bits
        {
            get { return data_bits; }
            set
            {
                if (data_bits != value)
                {
                    data_bits = value;
                    OnPropertyChanged(nameof(Data_bits));
                }
            }
        }

        private int stop_bits;
        public int Stop_bits
        {
            get { return stop_bits; }
            set
            {
                if (stop_bits != value)
                {
                    stop_bits = value;
                    OnPropertyChanged(nameof(Stop_bits));
                }
            }
        }

        private int flow_ctrl;
        public int Flow_ctrl
        {
            get { return flow_ctrl; }
            set
            {
                if (flow_ctrl != value)
                {
                    flow_ctrl = value;
                    OnPropertyChanged(nameof(Flow_ctrl));
                }
            }
        }

        private int parity;
        public int Parity
        {
            get { return parity; }
            set
            {
                if (parity != value)
                {
                    parity = value;
                    OnPropertyChanged(nameof(Parity));
                }
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;

        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }

    public class EthernetModel : INotifyPropertyChanged
    {
        private string ip;
        public string Ip
        {
            get { return ip; }
            set
            {
                if (ip != value)
                {
                    ip = value;
                    OnPropertyChanged(nameof(Ip));
                }
            }
        }

        private string nm;
        public string Nm
        {
            get { return nm; }
            set
            {
                if (nm != value)
                {
                    nm = value;
                    OnPropertyChanged(nameof(Nm));
                }
            }
        }

        private string gw;
        public string Gw
        {
            get { return gw; }
            set
            {
                if (gw != value)
                {
                    gw = value;
                    OnPropertyChanged(nameof(Gw));
                }
            }
        }

        private string dns;
        public string Dns
        {
            get { return dns; }
            set
            {
                if (dns != value)
                {
                    dns = value;
                    OnPropertyChanged(nameof(Dns));
                }
            }
        }

        private string mac;
        public string Mac
        {
            get { return mac; }
            set
            {
                if (mac != value)
                {
                    mac = value;
                    OnPropertyChanged(nameof(Mac));
                }
            }
        }

        private int lport;
        public int Lport
        {
            get { return lport; }
            set
            {
                if (lport != value)
                {
                    lport = value;
                    OnPropertyChanged(nameof(Lport));
                }
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;

        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }

    public class WlanModel : INotifyPropertyChanged
    {
        private string ip;
        public string Ip
        {
            get { return ip; }
            set
            {
                if (ip != value)
                {
                    ip = value;
                    OnPropertyChanged(nameof(Ip));
                }
            }
        }

        private string nm;
        public string Nm
        {
            get { return nm; }
            set
            {
                if (nm != value)
                {
                    nm = value;
                    OnPropertyChanged(nameof(Nm));
                }
            }
        }

        private string gw;
        public string Gw
        {
            get { return gw; }
            set
            {
                if (gw != value)
                {
                    gw = value;
                    OnPropertyChanged(nameof(Gw));
                }
            }
        }

        private string dns;
        public string Dns
        {
            get { return dns; }
            set
            {
                if (dns != value)
                {
                    dns = value;
                    OnPropertyChanged(nameof(Dns));
                }
            }
        }

        private string mac;
        public string Mac
        {
            get { return mac; }
            set
            {
                if (mac != value)
                {
                    mac = value;
                    OnPropertyChanged(nameof(Mac));
                }
            }
        }

        private int lport;
        public int Lport
        {
            get { return lport; }
            set
            {
                if (lport != value)
                {
                    lport = value;
                    OnPropertyChanged(nameof(Lport));
                }
            }
        }

        private int mode;
        public int Mode
        {
            get { return mode; }
            set
            {
                if (mode != value)
                {
                    mode = value;
                    OnPropertyChanged(nameof(Mode));
                }
            }
        }

        private string ssid;
        public string Ssid
        {
            get { return ssid; }
            set
            {
                if (ssid != value)
                {
                    ssid = value;
                    OnPropertyChanged(nameof(Ssid));
                }
            }
        }

        private string pwd;
        public string Pwd
        {
            get { return pwd; }
            set
            {
                if (pwd != value)
                {
                    pwd = value;
                    OnPropertyChanged(nameof(Pwd));
                }
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;

        protected virtual void OnPropertyChanged(string propertyName)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }



    #endregion

    
}
