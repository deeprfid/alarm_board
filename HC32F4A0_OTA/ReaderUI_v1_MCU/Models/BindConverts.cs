using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Data;
using ReaderManager;

namespace ReaderManager.Models
{

    public class DigitsConvert : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value == null)
            {
                return null;
            }
            string str = null;
            string ledDigit;
            foreach (var item in value as string)
            {
                switch (item)
                {
                    case '0':
                        ledDigit = "\ue618";//"&#xe000;"
                        break;
                    case '1':
                        ledDigit = "\ue60f";
                        break;
                    case '2':
                        ledDigit = "\ue610";
                        break;
                    case '3':
                        ledDigit = "\ue611";
                        break;
                    case '4':
                        ledDigit = "\ue612";
                        break;
                    case '5':
                        ledDigit = "\ue613";
                        break;
                    case '6':
                        ledDigit = "\ue614";
                        break;
                    case '7':
                        ledDigit = "\ue615";
                        break;
                    case '8':
                        ledDigit = "\ue616";
                        break;
                    case '9':
                        ledDigit = "\ue617";
                        break;
                    case '.':
                        ledDigit = ".";
                        //ledDigit = "\ue00d";//小数点转义
                        break;
                    default:
                        ledDigit = $"{item}";
                        break;
                }
                str += ledDigit;
            }
            return str;
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            return value;
        }
    }
    public static class Common {
        //是否是ip
        public static bool IsIPAddress(string str)
        {
            if (str == null || str == string.Empty || str.Length < 7 || str.Length > 15) return false;

            string regformat = @"^\d{1,3}[\.]\d{1,3}[\.]\d{1,3}[\.]\d{1,3}$";

            Regex regex = new Regex(regformat, RegexOptions.IgnoreCase);
            return regex.IsMatch(str);
        }
    }

    internal class AntCellColorConvert: IValueConverter
    {
        public object Convert(object value, Type typeTarget, object param, System.Globalization.CultureInfo culture)
        {
            int DicCnt = Application.Current.Resources.MergedDictionaries.Count;
            if (System.Convert.ToString(value) == 
                Application.Current.Resources.MergedDictionaries[DicCnt - 1]["ParamSettings_ParamAnts_AntInfo_Connected"].ToString())
                return "Green";
            else
                return "Red";
        }

        public object ConvertBack(object value, Type typeTarget, object param, System.Globalization.CultureInfo culture)
        {
            return value;
        }
    }

    internal class InvertBoolConvert : IValueConverter
    {
        public object Convert(object value, Type typeTarget, object param, System.Globalization.CultureInfo culture)
        {
            return !(bool)value;
        }

        public object ConvertBack(object value, Type typeTarget, object param, System.Globalization.CultureInfo culture)
        {
            return null;
        }
    }

    internal class BoolToNumConvert : IValueConverter
    {
        public object Convert(object value, Type typeTarget, object param, System.Globalization.CultureInfo culture)
        {
            if ((bool)value)
                return 0;
            else
                return 1;
        }

        public object ConvertBack(object value, Type typeTarget, object param, System.Globalization.CultureInfo culture)
        {
            return (int)value == 0;
        }
    }
}
