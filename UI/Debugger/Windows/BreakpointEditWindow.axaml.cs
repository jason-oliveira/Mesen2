using Avalonia;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;
using Mesen.Controls;
using Mesen.Debugger.ViewModels;
using Mesen.Utilities;
using System;
using System.Threading.Tasks;

namespace Mesen.Debugger.Windows
{
	public class BreakpointEditWindow : Window
	{
		public BreakpointEditWindow()
		{
			InitializeComponent();
#if DEBUG
			this.AttachDevTools();
#endif
		}

		private void InitializeComponent()
		{
			AvaloniaXamlLoader.Load(this);
		}

		protected override void OnOpened(EventArgs e)
		{
			base.OnOpened(e);
			this.GetControl<MesenNumericTextBox>("startAddress").Focus();
		}

		private void Ok_OnClick(object sender, RoutedEventArgs e)
		{
			Close(true);
		}

		private void Cancel_OnClick(object sender, RoutedEventArgs e)
		{
			Close(false);
		}

		public static async void EditBreakpoint(Breakpoint bp, Control parent)
		{
			await EditBreakpointAsync(bp, parent);
		}

		public static async Task<bool> EditBreakpointAsync(Breakpoint bp, Control parent)
		{
			Breakpoint copy = bp.Clone();
			BreakpointEditViewModel model = new BreakpointEditViewModel(copy);
			BreakpointEditWindow wnd = new BreakpointEditWindow() { DataContext = model };

			bool result = await wnd.ShowCenteredDialog<bool>(parent);
			if(result) {
				bp.CopyFrom(copy);
				BreakpointManager.AddBreakpoint(bp);
			}

			model.Dispose();

			return result;
		}
	}
}
