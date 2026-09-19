// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

using System.Windows.Controls;
using System.Windows.Media;
using MahApps.Metro.Controls;
using ReaderManager.Models;
using System.Linq;
using System.Windows;
namespace ReaderManager
{
    public sealed partial class HamburgerMenuRipple : UserControl
    {
        public HamburgerMenuRipple()
        {
            this.InitializeComponent();
            rootWnd = Application.Current.Windows.Cast<Window>()
            .FirstOrDefault(window => window is MainWindow) as MainWindow;
            rpViewModel = rootWnd.rdpmViewModel;
        }
        MainWindow rootWnd = null;
        ReaderParamsViewModel rpViewModel = null;

        int lastMenuItem = 0;
        private void HamburgerMenuControl_ItemInvoked(object sender, HamburgerMenuItemInvokedEventArgs args)
        {
            /*
            if (!args.IsItemOptions)
            {
                if (HamburgerMenuControl.SelectedIndex == 1)
                {
                    rootWnd.OpenRightFlyout(null, null);
                }
            }
            */
                      

            if (this.HamburgerMenuControl.SelectedIndex != 1 &&
                lastMenuItem ==1)
            {
                if (rpViewModel.IsInventory)
                {
                    rootWnd.TipSuccess(LangResouorce.GetText("Msg_Inventory_PlseStopInventory"));
                    this.HamburgerMenuControl.SelectedIndex = 1;
                    return;
                }
            }
            if (this.HamburgerMenuControl.SelectedIndex != 2 &&
                lastMenuItem == 2)
            {
                if (rpViewModel.IsReadTag)
                {
                    rootWnd.TipSuccess(LangResouorce.GetText("Msg_Inventory_PlseStopReadTag"));
                    this.HamburgerMenuControl.SelectedIndex = 1;
                    return;
                }
            }


            /*
            if (this.HamburgerMenuControl.SelectedIndex == 2)
                rootWnd.enableRightFlyBtn();
            else
            {
                rootWnd.disableRightFlyBtn();
            }
            */
            this.HamburgerMenuControl.Content = args.InvokedItem;
            lastMenuItem = this.HamburgerMenuControl.SelectedIndex;
        }

    }
}