using System.Collections.Generic;

namespace ReaderManager.Models
{
    /// <summary>
    /// 设备发现信息（UDP广播搜索结果）
    /// </summary>
    public class DevInfo
    {
        public DevInfo()
        {
            Ip = "";
            Mac = "";
            BoardType = "";
            ModType = "";
            NetMask = "";
            Gateway = "";
            Dns = "";
            Bver = "";
            Mver = "";
            Wmode = "";
        }

        public string No { get; set; }
        public string Ip { get; set; }
        public string Mac { get; set; }
        public string BoardType { get; set; }
        public string ModType { get; set; }
        public string NetMask { get; set; }
        public string Gateway { get; set; }
        public string Dns { get; set; }
        public int LisPort { get; set; }
        public string Bver { get; set; }
        public string Mver { get; set; }
        public string Wmode { get; set; }
        public int StateCode { get; set; }
        public bool IsDhcp { get; set; }
        public byte[] MacBytes { get; set; }
    }
}
