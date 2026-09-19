using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;

namespace ReaderManager.Models
{
    public class EditNumberValidation : ValidationRule
    {
        public enum ValidModeCode
        {
            NumValid_dadengyu0 = 0,
            NumValid_powerrange = 1,
        }
        
        public ValidModeCode vmode { get; set; }
        public ReaderParamsViewModel rpModel { get; set; }

        public override ValidationResult Validate(object value, CultureInfo cultureInfo)
        {
            int num;
            if (!int.TryParse(value.ToString(), out num))
                return new ValidationResult(false, LangResouorce.GetText("Msg_Validation_InvalidVal"));

            if (vmode == ValidModeCode.NumValid_dadengyu0)
            {
                if (num >= 0)
                    return new ValidationResult(true, null);
            }
            else if (vmode == ValidModeCode.NumValid_powerrange)
            {
                if (num >= rpModel.MinTxPower && num <= rpModel.MaxTxPower)
                    return new ValidationResult(true, null);
            }
            return new ValidationResult(false, LangResouorce.GetText("Msg_Validation_InvalidVal"));
        }
    }

    public class ValidatioAlgorithm
    {
        public static int validNumberParam(string strParam, string ctrlname, int min, int max, out int numParam)
        {
            MainWindow rootWnd = ReaderParamsViewModel.GetMainWindow();
            numParam = -1;
            if (strParam == string.Empty)
            {
                rootWnd.TipMessage(LangResouorce.GetText("Msg_PlsInput") + LangResouorce.GetText("Msg_Space") +
                    LangResouorce.GetText(ctrlname));
                return -1;
            }
            else
            {
                if (int.TryParse(strParam.Trim(), out numParam))
                {
                    if (numParam < min || numParam > max)
                    {
                        rootWnd.TipMessage(LangResouorce.GetText(ctrlname) +
                            LangResouorce.GetText("Msg_Validation_is") +
                            LangResouorce.GetText("Msg_Validation_InvalidVal"));
                        return -1;
                    }
                }
                else
                {
                    rootWnd.TipMessage(LangResouorce.GetText(ctrlname) +
                        LangResouorce.GetText("Msg_Validation_is") +
                        LangResouorce.GetText("Msg_Validation_InvalidVal"));
                    return -1;
                }
            }
            return 0;
        }

        public static int IsValidHexstr(string str, int len)
        {
            if (str == "")
                return -3;
            if (str.Length > len)
                return -4;
            string lowstr = str.ToLower();
            byte[] hexchars = Encoding.ASCII.GetBytes(lowstr);

            foreach (byte a in hexchars)
            {
                if (!((a >= 48 && a <= 57) || (a >= 97 && a <= 102)))
                    return -1;
            }
            return 0;
        }

        public static int IsValidBinaryStr(string str)
        {
            if (str == string.Empty)
                return -3;

            foreach (Char a in str)
            {
                if (!((a == '1') || (a == '0')))
                    return -1;
            }
            return 0;
        }
    }



    public class TagFilter_Mask_FormatValidation : ValidationRule
    {
        public override ValidationResult Validate(object value, CultureInfo cultureInfo)
        {
            if (tfVM.MaskFormat == 0)
            {
                int ret = ValidatioAlgorithm.IsValidBinaryStr(value.ToString().Trim());
                if (ret == -3)
                    return new ValidationResult(false, LangResouorce.GetText("Msg_Validation_InvalidEmpty"));
                else if (ret == -1)
                    return new ValidationResult(false, LangResouorce.GetText("Msg_Validation_InvalidVal"));
                else
                    return new ValidationResult(true, null);
            }
            if (tfVM.MaskFormat == 1)
            {
                int ret = ValidatioAlgorithm.IsValidHexstr(value.ToString().Trim(), 1000);
                if (ret == -3)
                    return new ValidationResult(false, LangResouorce.GetText("Msg_Validation_InvalidEmpty"));
                else if (ret == -1)
                    return new ValidationResult(false, LangResouorce.GetText("Msg_Validation_InvalidVal"));
                else
                    return new ValidationResult(true, null);
            }
            return new ValidationResult(false, LangResouorce.GetText("Msg_Validation_InvalidVal"));
        }
        
        public TagFilterViewModel tfVM { get; set; }
    }
}
