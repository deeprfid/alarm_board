using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ReaderManager.Models
{
    public static class EnumModel
    {
        public enum ProfileType
        {
            FM0 = 0,
            M2 = 1,
            M4 = 2,
            M8 = 3,
            PROFILE0 = 16,
            PROFILE1 = 17,
            PROFILE2 = 18,
            PROFILE3 = 19,
            PROFILE4 = 20,
            PROFILE5 = 21,
            RF_MODE_1 = 101,
            RF_MODE_3 = 103,
            RF_MODE_5 = 105,
            RF_MODE_7 = 107,
            RF_MODE_11 = 111,
            RF_MODE_12 = 112,
            RF_MODE_13 = 113,
            RF_MODE_15 = 115,
            RF_MODE_103 = 203,
            RF_MODE_120 = 220,
            RF_MODE_345 = 45
        }


        public static int ProfileTypeToValue(this ProfileType profileType)
        {
            return (int)profileType;
        }
        //// 使用枚举的示例
        //ProfileType profile = ProfileType.PROFILE1;
        //int value = profile.ToValue(); // value 将是 17

    }
  
}
