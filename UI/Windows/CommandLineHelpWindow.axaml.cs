using Avalonia;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;
using Avalonia.Threading;
using System;
using System.ComponentModel;
using Avalonia.Data;
using Mesen.Interop;
using System.Collections.Generic;
using Avalonia.Input;
using Mesen.Utilities;

namespace Mesen.Windows
{
	public class CommandLineHelpWindow : Window
	{
		public List<TabEntry> HelpTabs { get; } = new();

		public CommandLineHelpWindow()
		{
			Dictionary<string, string> switchesPerCategory = CommandLineHelper.GetAvailableSwitches();
			foreach(var kvp in switchesPerCategory) {
				HelpTabs.Add(new() { Name = kvp.Key, Content = kvp.Value });
			}

			InitializeComponent();
		}

		private void InitializeComponent()
		{
			AvaloniaXamlLoader.Load(this);
		}

		private void btnOk_OnClick(object? sender, RoutedEventArgs e)
		{
			Close();
		}

		public class TabEntry
		{
			public string Name { get; set; } = "";
			public string Content { get; set; } = "";
		}
	}
}