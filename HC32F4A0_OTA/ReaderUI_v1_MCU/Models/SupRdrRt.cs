using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using ModuleTech;
using System.Diagnostics;
using ReaderManager;
using System.Windows;
using System.Windows.Controls;

namespace ReaderManager.Models
{
    public class LangResouorce
    {
        public static string GetText(string key)
        {
            int DicCnt = Application.Current.Resources.MergedDictionaries.Count;
            return (string)Application.Current.Resources.MergedDictionaries[DicCnt - 1][key];
        }
    }
    public class CtrlListSources
    {

        public static void AddComboBox(ComboBox cbb)
        {
            langCbbList.Add(cbb);
        }

        static List<int> SelIndices = new List<int>();
        public static void BeforeSetLang()
        {
            SelIndices.Clear();
            foreach (ComboBox cbb in langCbbList)
            {
                SelIndices.Add(cbb.SelectedIndex);
            }
        }
        public static void RestoreIndices()
        {
            for (int i = 0; i < langCbbList.Count; ++i)
                langCbbList[i].SelectedIndex = SelIndices[i];
        }

        static List<ComboBox> langCbbList = new List<ComboBox>();
    }


    public class AntInfo: INotifyPropertyChanged
    {
        string Name_;
        int ReadPower_;
        int WritePower_;
        string ConnState_;
        bool IsUseInv_ = false;

        public event PropertyChangedEventHandler PropertyChanged;
        
        public int AntId { get; set; }
        public string Name
        {
            get { return Name_; }
            set
            {
                if (value != Name_)
                {
                    Name_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("Name"));
                }
            }
        }

        public int ReadPower
        {
            get { return ReadPower_; }
            set
            {
                if (value != ReadPower_)
                {
                    ReadPower_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("ReadPower"));
                }
            }
        }

        public int WritePower
        {
            get { return WritePower_; }
            set
            {
                if (value != WritePower_)
                {
                    WritePower_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("WritePower"));
                }
            }
        }

        public string ConnState
        {
            get { return ConnState_; }
            set
            {
                if (value != ConnState_)
                {
                    ConnState_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("ConnState"));
                }
            }
        }

        public bool IsUseInv
        {
            get { return IsUseInv_; }
            set
            {
                if (value != IsUseInv_)
                {
                    IsUseInv_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("IsUseInv"));
                }
            }
        }

    }
    public class TagInfo: INotifyPropertyChanged
    {
        int Reads_;       
        int Ant_;
        string BankData_;
        string Prot_;
        int Rssi_;
        int Freq_;
        string Phase_;
        
        public int Reads
        { 
            get { return Reads_; }
            set 
            {
                if (value != Reads_)
                {
                    Reads_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("Reads"));
                }
            } 
        }
        public string Epc { get; set; }
        public int Ant
        {
            get { return Ant_; }
            set
            {
                if (value != Ant_)
                {
                    Ant_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("Ant"));
                }
            }
        }
        public string BankData
        {
            get { return BankData_; }
            set
            {
                if (value != BankData_)
                {
                    BankData_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("BankData"));
                }
            }
        }
        public string Prot
        {
            get { return Prot_; }
            set
            {
                if (value != Prot_)
                {
                    Prot_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("Prot"));
                }
            }
        }
        public int Rssi
        {
            get { return Rssi_; }
            set
            {
                if (value != Rssi_)
                {
                    Rssi_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("Rssi"));
                }
            }
        }
        public int Freq
        {
            get { return Freq_; }
            set
            {
                if (value != Freq_)
                {
                    Freq_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("Freq"));
                }
            }
        }
        public string Phase
        {
            get { return Phase_; }
            set
            {
                if (value != Phase_)
                {
                    Phase_ = value;
                    if (PropertyChanged != null)
                        PropertyChanged.Invoke(this, new PropertyChangedEventArgs("Phase"));
                }
            }
        }

        public event PropertyChangedEventHandler PropertyChanged;
    }

    public class InventoryTags
    {
        public ObservableCollection<TagInfo> Tags = new ObservableCollection<TagInfo>();
        public InventoryTags()
        {
            /*
            for (int i = 0; i < 26; ++i)
            {
                TagInfo tmp = new TagInfo();
                tmp.Reads = i;
                string epc = "";
                for (int j = 0; j < 24; ++j)
                    epc += 'A'+i;
                tmp.Epc = epc;
                tmp.Ant = i+9;
                string bank = "";
                for (int j = 0; j < 16; ++j)
                    bank += j+i;
                tmp.BankData = bank;
                tmp.Prot = "Gen2";
                tmp.Freq = 920750;
                tmp.Phase = 90;
                Tags.Add(tmp);
            }
            */
        }



    }
}
