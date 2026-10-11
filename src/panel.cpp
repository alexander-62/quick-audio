// SPDX-License-Identifier: GPL-3.0-or-later
// XAML Diagnostics / COM TAP pattern adapted from m417z/UWPSpy (GPL-3.0).
#include "shared.h"
#include <ocidl.h>
#include <xamlom.h>
#include <mmdeviceapi.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Windows.System.h>
#include <memory>
#include <vector>
#include <atomic>
#include <cmath>
#include <thread>
namespace wf=winrt::Windows::Foundation;
namespace xaml=winrt::Windows::UI::Xaml;
namespace ctl=winrt::Windows::UI::Xaml::Controls;
namespace media=winrt::Windows::UI::Xaml::Media;
namespace core=winrt::Windows::UI::Core;
HMODULE moduleHandle;
std::atomic<bool> stopping{false};
std::shared_ptr<qa::Bus> sharedBus;
std::wstring Dir(){wchar_t p[32768];GetModuleFileNameW(moduleHandle,p,32768);std::wstring s(p);return s.substr(0,s.find_last_of(L'\\')+1);}
void Log(std::wstring const& s){
    if(qa::Log(Dir(),s))return;
    auto h=CreateFileW((Dir()+L"QuickAudio.Hook.log").c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h!=INVALID_HANDLE_VALUE){auto w=s+L"\r\n";int size=WideCharToMultiByte(CP_UTF8,0,w.c_str(),static_cast<int>(w.size()),nullptr,0,nullptr,nullptr);std::string bytes(size,0);WideCharToMultiByte(CP_UTF8,0,w.c_str(),static_cast<int>(w.size()),bytes.data(),size,nullptr,nullptr);DWORD written;WriteFile(h,bytes.data(),size,&written,nullptr);CloseHandle(h);}
}
std::wstring Error(){wchar_t b[32];swprintf_s(b,L"0x%08X",(unsigned)winrt::to_hresult());return b;}
media::SolidColorBrush Brush(winrt::Windows::UI::Color c){return media::SolidColorBrush(c);}
void AutoRow(ctl::Grid const& grid){ctl::RowDefinition r;r.Height({1,xaml::GridUnitType::Auto});grid.RowDefinitions().Append(r);}
void Column(ctl::Grid const& grid,double size,bool star=false){ctl::ColumnDefinition c;c.Width({size,star?xaml::GridUnitType::Star:xaml::GridUnitType::Pixel});grid.ColumnDefinitions().Append(c);}
ctl::TextBlock Label(std::wstring const& s,double size=14){ctl::TextBlock t;t.Text(s);t.FontSize(size);return t;}
struct Row {qa::Device device{};ctl::Button button{nullptr};ctl::Border stripe{nullptr};ctl::FontIcon check{nullptr};ctl::TextBlock text{nullptr};};
struct Section {ctl::StackPanel items{nullptr};ctl::ScrollViewer scroll{nullptr};ctl::TextBlock empty{nullptr};std::vector<Row> rows;};
struct SettingRow {
    qa::Device device{};
    ctl::CheckBox checkbox{nullptr};ctl::TextBlock label{nullptr};
    ctl::TextBox editor{nullptr};ctl::Button save{nullptr};ctl::FontIcon actionIcon{nullptr};
    bool editing=false;unsigned long long pendingRename=0;
};
struct View:std::enable_shared_from_this<View>{
    winrt::weak_ref<ctl::Grid> parent;
    ctl::Border panel{nullptr};ctl::StackPanel content{nullptr},devicesPage{nullptr},settingsPage{nullptr};
    Section sections[2];ctl::CheckBox sync{nullptr};ctl::TextBlock status{nullptr};ctl::ProgressRing progress{nullptr};
    ctl::Button settingsButton{nullptr};ctl::ScrollViewer settingsScroll{nullptr};ctl::CheckBox startup{nullptr};ctl::StackPanel settingsItems[2]{nullptr,nullptr};std::vector<SettingRow> settingRows[2];
    ctl::RowDefinition extraRow{nullptr};
    std::vector<std::pair<winrt::weak_ref<xaml::FrameworkElement>,int>> shifted;
    std::vector<std::pair<winrt::weak_ref<xaml::FrameworkElement>,wf::IInspectable>> heights;
    std::shared_ptr<qa::State> latest;
    bool updating=false,updatingSettings=false,settingsOpen=false,detached=false;
    std::wstring token;
    void Feedback(bool busy,std::wstring const& error=L""){
        // Both indicators occupy the same permanent slot beside the checkbox.
        // Switching never adds a row or changes the panel's measured size.
        progress.IsActive(busy);progress.Visibility(busy?xaml::Visibility::Visible:xaml::Visibility::Collapsed);
        status.Visibility(error.empty()?xaml::Visibility::Collapsed:xaml::Visibility::Visible);
        if(!error.empty()){ctl::ToolTipService::SetToolTip(status,winrt::box_value(error));xaml::Automation::AutomationProperties::SetName(status,error);}
    }
    bool Request(DWORD kind,DWORD flow,std::wstring const& id,std::wstring const& name=L"",unsigned long long* requestNumber=nullptr){
        auto bus=std::atomic_load(&sharedBus);
        if(bus && bus->Send(kind,flow,id,name,requestNumber)){
            Feedback(true);for(auto& group:settingRows)for(auto& row:group){row.checkbox.IsEnabled(false);row.save.IsEnabled(false);row.editor.IsEnabled(false);}
            Log(L"UI_REQUEST kind="+std::to_wstring(kind)+L" flow="+std::to_wstring(flow)+L" id="+id+L" panelHeight="+std::to_wstring(panel.ActualHeight()));return true;
        }
        Feedback(false,L"Подождите завершения переключения и попробуйте снова.");return false;
    }
    void SetEditing(SettingRow& row,bool editing,bool focus=false){
        row.editing=editing;
        row.label.Visibility(editing?xaml::Visibility::Collapsed:xaml::Visibility::Visible);
        row.editor.Visibility(editing?xaml::Visibility::Visible:xaml::Visibility::Collapsed);
        row.actionIcon.Glyph(editing?L"\uE73E":L"\uE70F");
        std::wstring action=editing?L"Сохранить название":L"Переименовать устройство";
        ctl::ToolTipService::SetToolTip(row.save,winrt::box_value(action));
        xaml::Automation::AutomationProperties::SetName(row.save,action+L": "+std::wstring(row.device.name));
        if(editing){
            row.editor.Text(row.device.alias[0]?row.device.alias:row.device.name);
            if(focus){row.editor.Focus(xaml::FocusState::Programmatic);row.editor.SelectAll();}
        }else if(focus)row.save.Focus(xaml::FocusState::Programmatic);
    }
    void RenameAction(DWORD flow,std::wstring const& id){
        for(auto& row:settingRows[flow])if(id==row.device.id){
            if(!row.save.IsEnabled() || row.pendingRename)return;
            if(row.editing){Request(6,flow,id,std::wstring(row.editor.Text()),&row.pendingRename);}
            else{
                for(auto& group:settingRows)for(auto& other:group)if(other.editing && !other.pendingRename)SetEditing(other,false);
                SetEditing(row,true,true);
            }
            return;
        }
    }
    void CancelRename(DWORD flow,std::wstring const& id){
        for(auto& row:settingRows[flow])if(id==row.device.id && row.editing && !row.pendingRename){SetEditing(row,false,true);return;}
    }
    void ShowSettings(bool show){
        if(show){
            auto foreground=GetForegroundWindow();MONITORINFO monitor{sizeof(monitor)};
            if(GetMonitorInfoW(MonitorFromWindow(foreground,MONITOR_DEFAULTTONEAREST),&monitor)){
                double dpi=GetDpiForWindow(foreground);if(dpi<=0)dpi=96;
                double available=(monitor.rcWork.bottom-monitor.rcWork.top)*96.0/dpi;
                double native=333;if(auto grid=parent.get())native=std::max(0.0,grid.ActualHeight()-panel.ActualHeight());
                settingsScroll.MaxHeight(std::clamp(available-native-130,96.0,240.0));
            }
        }
        if(!show)for(auto& group:settingRows)for(auto& row:group)if(row.editing && !row.pendingRename)SetEditing(row,false);
        settingsOpen=show;devicesPage.Visibility(show?xaml::Visibility::Collapsed:xaml::Visibility::Visible);settingsPage.Visibility(show?xaml::Visibility::Visible:xaml::Visibility::Collapsed);
        Log(show?L"SETTINGS_OPEN inline=1":L"SETTINGS_CLOSE");
    }
    void BuildSettings(){
        auto weak=weak_from_this();settingsPage=ctl::StackPanel();settingsPage.Spacing(6);settingsPage.Visibility(xaml::Visibility::Collapsed);content.Children().Append(settingsPage);
        ctl::Grid heading;Column(heading,36);Column(heading,1,true);ctl::Button back;back.Content(winrt::box_value(L"\u2190"));back.Width(32);back.Height(32);back.MinWidth(0);back.Padding({0,0,0,0});heading.Children().Append(back);
        xaml::Automation::AutomationProperties::SetName(back,L"Вернуться к выбору устройств");back.Click([weak](wf::IInspectable const&,xaml::RoutedEventArgs const&){if(auto self=weak.lock())self->ShowSettings(false);});
        auto title=Label(L"QuickAudio " QA_VERSION_W,15);title.FontWeight({600});title.VerticalAlignment(xaml::VerticalAlignment::Center);ctl::Grid::SetColumn(title,1);heading.Children().Append(title);settingsPage.Children().Append(heading);
        settingsScroll=ctl::ScrollViewer();settingsScroll.MaxHeight(240);settingsScroll.VerticalScrollBarVisibility(ctl::ScrollBarVisibility::Auto);settingsScroll.HorizontalScrollBarVisibility(ctl::ScrollBarVisibility::Disabled);
        ctl::StackPanel groups;groups.Spacing(6);settingsScroll.Content(groups);settingsPage.Children().Append(settingsScroll);
        startup=ctl::CheckBox();startup.Content(winrt::box_value(L"Запускать вместе с Windows"));startup.FontSize(13);groups.Children().Append(startup);
        startup.Checked([weak](wf::IInspectable const&,xaml::RoutedEventArgs const&){if(auto self=weak.lock())if(!self->updatingSettings)self->Request(7,1,L"");});
        startup.Unchecked([weak](wf::IInspectable const&,xaml::RoutedEventArgs const&){if(auto self=weak.lock())if(!self->updatingSettings)self->Request(7,0,L"");});
        auto hint=Label(L"Флажок показывает устройство. Карандаш — переименовать, \u2713 или Enter — сохранить, Esc — отменить. Пустое имя вернёт исходное.",12);hint.TextWrapping(xaml::TextWrapping::Wrap);hint.Opacity(0.75);groups.Children().Append(hint);
        for(int f=0;f<2;++f){auto header=Label(f?L"Микрофоны":L"Устройства вывода",13);header.FontWeight({600});header.Margin({0,8,0,0});groups.Children().Append(header);settingsItems[f]=ctl::StackPanel();settingsItems[f].Spacing(4);groups.Children().Append(settingsItems[f]);}
        settingsButton.Click([weak](wf::IInspectable const&,xaml::RoutedEventArgs const&){if(auto self=weak.lock())self->ShowSettings(!self->settingsOpen);});
    }
    void UpdateSettings(qa::State const& state,bool busy){
        updatingSettings=true;auto weak=weak_from_this();
        for(DWORD f=0;f<2;++f){
            auto& rows=settingRows[f];bool rebuild=rows.size()!=state.count[f];
            if(!rebuild)for(DWORD i=0;i<state.count[f];++i)if(wcscmp(rows[i].device.id,state.devices[f][i].id) || wcscmp(rows[i].device.name,state.devices[f][i].name)){rebuild=true;break;}
            if(rebuild){settingsItems[f].Children().Clear();rows.clear();
                for(DWORD i=0;i<state.count[f];++i){
                    SettingRow row;row.device=state.devices[f][i];
                    ctl::Grid entry;entry.Height(36);Column(entry,28);Column(entry,1,true);Column(entry,36);
                    row.checkbox=ctl::CheckBox();row.checkbox.MinHeight(0);row.checkbox.MinWidth(0);row.checkbox.Width(24);row.checkbox.Height(32);row.checkbox.VerticalAlignment(xaml::VerticalAlignment::Center);entry.Children().Append(row.checkbox);
                    row.label=Label(row.device.name,13);row.label.TextWrapping(xaml::TextWrapping::NoWrap);row.label.TextTrimming(xaml::TextTrimming::CharacterEllipsis);row.label.VerticalAlignment(xaml::VerticalAlignment::Center);row.label.Margin({0,0,6,0});ctl::Grid::SetColumn(row.label,1);entry.Children().Append(row.label);
                    std::wstring id(row.device.id);
                    row.checkbox.Checked([weak,id,f](wf::IInspectable const&,xaml::RoutedEventArgs const&){if(auto self=weak.lock())if(!self->updatingSettings)self->Request(5,f,id);});
                    row.checkbox.Unchecked([weak,id,f](wf::IInspectable const&,xaml::RoutedEventArgs const&){if(auto self=weak.lock())if(!self->updatingSettings)self->Request(4,f,id);});
                    row.editor=ctl::TextBox();row.editor.FontSize(13);row.editor.MaxLength(255);row.editor.Height(32);row.editor.MinHeight(0);row.editor.Padding({6,4,6,4});row.editor.VerticalAlignment(xaml::VerticalAlignment::Center);row.editor.Margin({0,0,4,0});row.editor.PlaceholderText(row.device.name);row.editor.IsSpellCheckEnabled(false);row.editor.AcceptsReturn(false);row.editor.TextWrapping(xaml::TextWrapping::NoWrap);ctl::Grid::SetColumn(row.editor,1);entry.Children().Append(row.editor);
                    xaml::Automation::AutomationProperties::SetName(row.editor,L"Название в панели: "+std::wstring(row.device.name));
                    row.save=ctl::Button();row.save.MinWidth(0);row.save.MinHeight(0);row.save.Padding({0,0,0,0});row.save.Width(32);row.save.Height(32);row.save.VerticalAlignment(xaml::VerticalAlignment::Center);ctl::Grid::SetColumn(row.save,2);entry.Children().Append(row.save);
                    row.actionIcon=ctl::FontIcon();row.actionIcon.FontFamily(media::FontFamily(L"Segoe Fluent Icons"));row.actionIcon.FontSize(14);row.save.Content(row.actionIcon);SetEditing(row,false);
                    row.save.Click([weak,id,f](wf::IInspectable const&,xaml::RoutedEventArgs const&){if(auto self=weak.lock())self->RenameAction(f,id);});
                    row.editor.KeyDown([weak,id,f](wf::IInspectable const&,xaml::Input::KeyRoutedEventArgs const& args){
                        if(args.Key()==winrt::Windows::System::VirtualKey::Enter){args.Handled(true);if(auto self=weak.lock())self->RenameAction(f,id);}
                        else if(args.Key()==winrt::Windows::System::VirtualKey::Escape){args.Handled(true);if(auto self=weak.lock())self->CancelRename(f,id);}
                    });
                    settingsItems[f].Children().Append(entry);rows.push_back(std::move(row));
                }
                if(rows.empty())settingsItems[f].Children().Append(Label(L"Доступных устройств нет",12));
            }
            for(DWORD i=0;i<state.count[f];++i){
                auto& row=rows[i];row.device=state.devices[f][i];
                std::wstring display(row.device.alias[0]?row.device.alias:row.device.name);
                row.label.Text(display);std::wstring tooltip=display;if(row.device.alias[0])tooltip+=L"\n"+std::wstring(row.device.name);
                ctl::ToolTipService::SetToolTip(row.label,winrt::box_value(tooltip));ctl::ToolTipService::SetToolTip(row.checkbox,winrt::box_value(L"Показывать в панели: "+display));
                xaml::Automation::AutomationProperties::SetName(row.checkbox,L"Показывать в панели: "+display);
                row.checkbox.IsChecked(winrt::box_value(row.device.hidden==0).as<wf::IReference<bool>>());row.checkbox.IsEnabled(!busy);row.save.IsEnabled(!busy);row.editor.IsEnabled(!busy);
                // Older queued snapshots must not close a freshly opened editor.
                if(row.pendingRename && state.completed>=row.pendingRename){
                    row.pendingRename=0;
                    if(SUCCEEDED(static_cast<HRESULT>(state.lastHresult))){
                        auto focused=xaml::Input::FocusManager::GetFocusedElement();bool restoreFocus=settingsOpen && (focused==row.editor || focused==row.save);
                        SetEditing(row,false,restoreFocus);
                    }else if(settingsOpen)row.editor.Focus(xaml::FocusState::Programmatic);
                }
            }
        }startup.IsChecked(winrt::box_value(state.autostart!=0).as<wf::IReference<bool>>());startup.IsEnabled(!busy);updatingSettings=false;
    }
    void Attach(ctl::Grid const& footer){
        auto grid=footer.Parent().try_as<ctl::Grid>();
        if(!grid || grid.Name()!=L"L1Grid")throw winrt::hresult_error(E_FAIL,L"Unsupported Quick Settings layout");
        for(auto child:grid.Children()){auto f=child.try_as<xaml::FrameworkElement>();if(f && f.Name()==L"QuickAudioPanel")return;}
        int footerRow=ctl::Grid::GetRow(footer),insertion=0;
        Log(L"LAYOUT rows="+std::to_wstring(grid.RowDefinitions().Size())+L" footerRow="+std::to_wstring(footerRow)+L" height="+std::to_wstring(grid.Height())+L" max="+std::to_wstring(grid.MaxHeight())+L" actual="+std::to_wstring(grid.ActualHeight()));
        if(footerRow<0 || footerRow>=(int)grid.RowDefinitions().Size())throw winrt::hresult_error(E_FAIL,L"Missing footer row");
        parent=winrt::make_weak(grid);
        panel=ctl::Border();panel.Name(L"QuickAudioPanel");panel.Margin({20,14,20,8});panel.Padding({0,0,0,0});
        content=ctl::StackPanel();content.Spacing(6);panel.Child(content);
        xaml::Automation::AutomationProperties::SetAutomationId(panel,L"QuickAudioPanel");
        devicesPage=ctl::StackPanel();devicesPage.Spacing(6);content.Children().Append(devicesPage);
        for(int f=0;f<2;++f){
            auto heading=Label(f?L"Микрофоны":L"Устройства вывода",14);
            heading.FontWeight({600});heading.Margin({0,f?6.0:0.0,0,2});devicesPage.Children().Append(heading);
            auto& section=sections[f];section.items=ctl::StackPanel();section.items.Spacing(2);
            section.scroll=ctl::ScrollViewer();section.scroll.HorizontalScrollBarVisibility(ctl::ScrollBarVisibility::Disabled);section.scroll.VerticalScrollBarVisibility(ctl::ScrollBarVisibility::Auto);
            section.scroll.Content(section.items);section.scroll.MaxHeight(f?92:138);
            xaml::Automation::AutomationProperties::SetName(section.scroll,f?L"Выбор микрофона":L"Выбор устройства вывода звука");
            devicesPage.Children().Append(section.scroll);
            section.empty=Label(L"Доступных устройств нет",13);section.empty.Opacity(0.65);devicesPage.Children().Append(section.empty);
        }
        sync=ctl::CheckBox();sync.Content(winrt::box_value(L"Также для звонков"));sync.FontSize(13);sync.MinHeight(32);
        ctl::ToolTipService::SetToolTip(sync,winrt::box_value(L"При выборе менять также устройство связи, используемое приложениями для звонков."));
        auto weak=weak_from_this();
        sync.Checked([weak](wf::IInspectable const&,xaml::RoutedEventArgs const&){if(auto self=weak.lock())if(!self->updating)self->Request(2,1,L"");});
        sync.Unchecked([weak](wf::IInspectable const&,xaml::RoutedEventArgs const&){if(auto self=weak.lock())if(!self->updating)self->Request(2,0,L"");});
        ctl::Grid options;options.Height(32);Column(options,1,true);Column(options,24);Column(options,32);options.Children().Append(sync);
        ctl::Grid indicator;indicator.Width(24);indicator.Height(32);ctl::Grid::SetColumn(indicator,1);options.Children().Append(indicator);
        progress=ctl::ProgressRing();progress.Width(18);progress.Height(18);progress.MinWidth(0);progress.MinHeight(0);
        progress.HorizontalAlignment(xaml::HorizontalAlignment::Center);progress.VerticalAlignment(xaml::VerticalAlignment::Center);
        ctl::ToolTipService::SetToolTip(progress,winrt::box_value(L"Применяем выбор устройства"));indicator.Children().Append(progress);
        status=Label(L"\u26A0",15);status.HorizontalAlignment(xaml::HorizontalAlignment::Center);status.VerticalAlignment(xaml::VerticalAlignment::Center);
        status.Foreground(Brush({255,230,160,45}));indicator.Children().Append(status);
        settingsButton=ctl::Button();settingsButton.Width(28);settingsButton.Height(28);settingsButton.MinWidth(0);settingsButton.MinHeight(0);settingsButton.Padding({2,2,2,2});
        settingsButton.VerticalAlignment(xaml::VerticalAlignment::Center);ctl::Grid::SetColumn(settingsButton,2);options.Children().Append(settingsButton);
        ctl::FontIcon gear;gear.FontFamily(media::FontFamily(L"Segoe Fluent Icons"));gear.Glyph(L"\uE713");gear.FontSize(16);settingsButton.Content(gear);
        ctl::ToolTipService::SetToolTip(settingsButton,winrt::box_value(L"Настройки аудиопанели"));xaml::Automation::AutomationProperties::SetName(settingsButton,L"Настройки аудиопанели");
        BuildSettings();
        content.Children().Append(options);Feedback(false);
        ctl::Border divider;divider.Height(1);divider.Background(Brush({55,128,128,128}));divider.Margin({0,3,0,0});content.Children().Append(divider);
        extraRow=ctl::RowDefinition();extraRow.Height({1,xaml::GridUnitType::Auto});
        for(auto child:grid.Children()){
            if(auto f=child.try_as<xaml::FrameworkElement>()){int row=ctl::Grid::GetRow(f);if(row>=insertion){shifted.push_back({winrt::make_weak(f),row});ctl::Grid::SetRow(f,row+1);}}
        }
        grid.RowDefinitions().InsertAt(insertion,extraRow);ctl::Grid::SetRow(panel,insertion);grid.Children().Append(panel);
        // Save local values so disabling restores the native layout.
        xaml::FrameworkElement cursor=grid;
        for(int depth=0;cursor && depth<5;++depth){
            if(!std::isnan(cursor.Height())){heights.push_back({winrt::make_weak(cursor),cursor.ReadLocalValue(xaml::FrameworkElement::HeightProperty())});cursor.Height(NAN);}
            cursor=cursor.Parent().try_as<xaml::FrameworkElement>();
        }
        grid.InvalidateMeasure();
        panel.Loaded([weak](wf::IInspectable const&,xaml::RoutedEventArgs const&){if(auto self=weak.lock()){auto b=std::atomic_load(&sharedBus);if(b)b->Send(3,0,L"");}});
        panel.SizeChanged([weak](wf::IInspectable const&,xaml::SizeChangedEventArgs const& e){if(auto self=weak.lock())Log(L"UI_SIZE height="+std::to_wstring(e.NewSize().Height)+L" width="+std::to_wstring(e.NewSize().Width));});
        Log(L"UI_INSERTED parent=L1Grid row="+std::to_wstring(insertion)+L" parentMatches="+std::to_wstring(panel.Parent()==grid));
    }
    Row MakeRow(qa::Device const& d,DWORD flow){
        Row row;row.device=d;row.button=ctl::Button();row.button.MinHeight(42);row.button.Padding({0,0,8,0});
        row.button.HorizontalAlignment(xaml::HorizontalAlignment::Stretch);row.button.HorizontalContentAlignment(xaml::HorizontalAlignment::Stretch);
        row.button.BorderThickness({0,0,0,0});row.button.CornerRadius({4,4,4,4});
        ctl::Grid body;Column(body,4);Column(body,30);Column(body,1,true);Column(body,20);
        row.stripe=ctl::Border();row.stripe.Width(3);row.stripe.Height(24);row.stripe.CornerRadius({2,2,2,2});row.stripe.VerticalAlignment(xaml::VerticalAlignment::Center);body.Children().Append(row.stripe);
        ctl::FontIcon icon;icon.FontFamily(media::FontFamily(L"Segoe Fluent Icons"));icon.FontSize(16);
        icon.Glyph(flow?L"\uE720":(d.form==Headphones || d.form==Headset)?L"\uE7F4":d.form==DigitalAudioDisplayDevice?L"\uE7F8":L"\uE767");
        ctl::Grid::SetColumn(icon,1);body.Children().Append(icon);
        row.text=Label(d.alias[0]?d.alias:d.name,13);row.text.TextTrimming(xaml::TextTrimming::CharacterEllipsis);row.text.VerticalAlignment(xaml::VerticalAlignment::Center);ctl::Grid::SetColumn(row.text,2);body.Children().Append(row.text);
        row.check=ctl::FontIcon();row.check.FontFamily(media::FontFamily(L"Segoe Fluent Icons"));row.check.Glyph(L"\uE73E");row.check.FontSize(12);ctl::Grid::SetColumn(row.check,3);body.Children().Append(row.check);
        row.button.Content(body);row.button.Tag(winrt::box_value(std::wstring(d.id)));
        xaml::Automation::AutomationProperties::SetAutomationId(row.button,L"QuickAudioDevice_"+std::to_wstring(flow)+L"_"+d.id);
        auto weak=weak_from_this();std::wstring id(d.id);
        row.button.Click([weak,id,flow](wf::IInspectable const&,xaml::RoutedEventArgs const&){if(auto self=weak.lock())self->Request(1,flow,id);});
        return row;
    }
    void Apply(std::shared_ptr<qa::State> const& state){
        if(detached || !panel)return;latest=state;updating=true;
        bool busy=state->request!=state->completed;
        auto accent=winrt::Windows::UI::ViewManagement::UISettings().GetColorValue(winrt::Windows::UI::ViewManagement::UIColorType::Accent);
        for(DWORD f=0;f<2;++f){
            std::vector<qa::Device> visible;for(DWORD i=0;i<state->count[f];++i)if(!state->devices[f][i].hidden)visible.push_back(state->devices[f][i]);
            auto& section=sections[f];bool rebuild=section.rows.size()!=visible.size();
            if(!rebuild)for(size_t i=0;i<visible.size();++i)if(wcscmp(section.rows[i].device.id,visible[i].id) || wcscmp(section.rows[i].device.name,visible[i].name) || wcscmp(section.rows[i].device.alias,visible[i].alias) || section.rows[i].device.form!=visible[i].form){rebuild=true;break;}
            if(rebuild){
                std::wstring focus;auto focused=xaml::Input::FocusManager::GetFocusedElement().try_as<ctl::Button>();
                if(focused && focused.Tag())focus=winrt::unbox_value_or<winrt::hstring>(focused.Tag(),L"");
                double offset=section.scroll.VerticalOffset();section.items.Children().Clear();section.rows.clear();
                for(auto const& device:visible){auto row=MakeRow(device,f);section.items.Children().Append(row.button);section.rows.push_back(std::move(row));}
                section.scroll.ChangeView(nullptr,winrt::box_value(offset).as<wf::IReference<double>>(),nullptr,true);
                if(!focus.empty())for(auto& row:section.rows)if(focus==row.device.id)row.button.Focus(xaml::FocusState::Programmatic);
            }
            section.empty.Text(state->count[f]?L"Все устройства скрыты — откройте настройки":L"Доступных устройств нет");section.empty.TextWrapping(xaml::TextWrapping::Wrap);
            section.empty.Visibility(visible.empty()?xaml::Visibility::Visible:xaml::Visibility::Collapsed);
            section.scroll.Visibility(visible.empty()?xaml::Visibility::Collapsed:xaml::Visibility::Visible);
            for(auto& row:section.rows){
                bool selected=wcscmp(row.device.id,state->defaults[f][eMultimedia])==0;
                bool calls=wcscmp(row.device.id,state->defaults[f][eCommunications])==0;
                row.stripe.Background(Brush(accent));row.stripe.Visibility(selected?xaml::Visibility::Visible:xaml::Visibility::Collapsed);
                row.check.Visibility(selected?xaml::Visibility::Visible:xaml::Visibility::Collapsed);
                row.button.Background(Brush(selected?winrt::Windows::UI::Color{35,128,128,128}:winrt::Windows::UI::Color{0,0,0,0}));
                row.button.IsEnabled(!busy);
                std::wstring title(row.device.alias[0]?row.device.alias:row.device.name);if(row.device.alias[0])title+=L"\n"+std::wstring(row.device.name);if(selected)title+=L" — выбрано";if(calls)title+=L"; для звонков";
                ctl::ToolTipService::SetToolTip(row.button,winrt::box_value(title));xaml::Automation::AutomationProperties::SetName(row.button,title);
            }
        }
        sync.IsChecked(winrt::box_value(state->syncCalls!=0).as<wf::IReference<bool>>());sync.IsEnabled(!busy);
        UpdateSettings(*state,busy);
        Feedback(busy,state->status);
        updating=false;
        if(auto grid=parent.get())grid.InvalidateMeasure();
        Log(L"UI_STATE revision="+std::to_wstring(state->revision)+L" outputs="+std::to_wstring(state->count[0])+L" microphones="+std::to_wstring(state->count[1])+L" busy="+std::to_wstring(busy)+L" panelHeight="+std::to_wstring(panel.ActualHeight()));
    }
    void Detach(){
        if(detached)return;detached=true;
        try{
            if(auto grid=parent.get()){
                uint32_t index;if(panel && grid.Children().IndexOf(panel,index))grid.Children().RemoveAt(index);
                if(extraRow && grid.RowDefinitions().IndexOf(extraRow,index))grid.RowDefinitions().RemoveAt(index);
                for(auto& entry:shifted)if(auto f=entry.first.get())ctl::Grid::SetRow(f,entry.second);
                for(auto& entry:heights)if(auto f=entry.first.get()){if(entry.second==xaml::DependencyProperty::UnsetValue())f.ClearValue(xaml::FrameworkElement::HeightProperty());else f.SetValue(xaml::FrameworkElement::HeightProperty(),entry.second);}
                grid.InvalidateMeasure();
            }
            settingsPage=nullptr;devicesPage=nullptr;settingsScroll=nullptr;startup=nullptr;settingsButton=nullptr;for(int f=0;f<2;++f){settingRows[f].clear();settingsItems[f]=nullptr;}
            panel=nullptr;content=nullptr;for(auto& s:sections){s.rows.clear();s.items=nullptr;s.scroll=nullptr;s.empty=nullptr;}sync=nullptr;status=nullptr;progress=nullptr;
            Log(L"UI_REMOVED nativeLayoutRestored=1");
        }catch(...){Log(L"DETACH_ERROR "+Error());}
    }
};
std::mutex viewsLock;
std::vector<std::shared_ptr<View>> views;
void ForViews(std::shared_ptr<qa::State> state,bool remove=false){
    std::vector<std::shared_ptr<View>> snapshot;{std::lock_guard<std::mutex> lock(viewsLock);snapshot=views;if(remove)views.clear();}
    for(auto view:snapshot)try{
        if(auto grid=view->parent.get())grid.Dispatcher().RunAsync(core::CoreDispatcherPriority::Normal,[view,state,remove]{try{if(remove)view->Detach();else view->Apply(state);}catch(...){view->updating=false;view->updatingSettings=false;Log(L"UI_ERROR "+Error());}});
    }catch(...){Log(L"DISPATCH_ERROR "+Error());}
}
void Listen(){
    try{
        auto bus=std::make_shared<qa::Bus>();if(!bus->Open(false)){Log(L"BUS_OPEN_ERROR "+std::to_wstring(GetLastError()));return;}std::atomic_store(&sharedBus,bus);
        unsigned long long previous=~0ULL,previousGeneration=0;DWORD owner=0;HANDLE process=nullptr;
        while(!stopping){
            auto state=std::make_shared<qa::State>();
            if(bus->Snapshot(*state)){
                if(state->stopping)break;
                if(state->ownerPid!=owner){if(process)CloseHandle(process);owner=state->ownerPid;process=OpenProcess(SYNCHRONIZE,FALSE,owner);}
                if(process && WaitForSingleObject(process,0)==WAIT_OBJECT_0)break;
                if(state->revision!=previous || state->generation!=previousGeneration){previous=state->revision;previousGeneration=state->generation;ForViews(state);}
            }
            HANDLE events[]{bus->stop,bus->changed};if(WaitForMultipleObjects(2,events,FALSE,500)==WAIT_OBJECT_0)break;
        }
        if(process)CloseHandle(process);stopping=true;ForViews(nullptr,true);Log(L"LISTENER_STOP");
    }catch(...){Log(L"LISTENER_ERROR "+Error());stopping=true;ForViews(nullptr,true);}
}
struct Watcher:winrt::implements<Watcher,IVisualTreeServiceCallback2,winrt::non_agile>{
    winrt::com_ptr<IXamlDiagnostics> diag;explicit Watcher(winrt::com_ptr<IUnknown> const& site):diag(site.as<IXamlDiagnostics>()){}
    void Start(){AddRef();auto thread=CreateThread(nullptr,0,[](void* p)->DWORD{
        auto self=static_cast<Watcher*>(p);try{auto service=self->diag.as<IVisualTreeService3>();auto hr=service->AdviseVisualTreeChange(self);Log(L"XAML_ADVISE hr="+std::to_wstring((unsigned)hr));
            while(SUCCEEDED(hr) && !stopping)Sleep(250);if(SUCCEEDED(hr)){service->UnadviseVisualTreeChange(self);Log(L"XAML_UNADVISE");}
        }catch(...){Log(L"XAML_ERROR "+Error());}self->Release();return 0;
    },this,0,nullptr);if(thread)CloseHandle(thread);else Release();}
    HRESULT STDMETHODCALLTYPE OnVisualTreeChange(ParentChildRelation,VisualElement element,VisualMutationType mutation) noexcept override{
        try{
            if(mutation!=Add || stopping)return S_OK;
            wf::IInspectable obj{nullptr};winrt::check_hresult(diag->GetIInspectableFromHandle(element.Handle,reinterpret_cast<::IInspectable**>(winrt::put_abi(obj))));
            auto f=obj.try_as<xaml::FrameworkElement>();if(!f || f.Name()!=L"FooterGrid")return S_OK;
            auto footer=obj.try_as<ctl::Grid>();if(!footer)return S_OK;
            auto weak=winrt::make_weak(footer);footer.Dispatcher().RunAsync(core::CoreDispatcherPriority::Normal,[weak]{
                try{
                    if(stopping)return;auto footer=weak.get();if(!footer)return;
                    auto parent=footer.Parent().try_as<ctl::Grid>();if(!parent)return;
                    for(auto child:parent.Children()){auto f=child.try_as<xaml::FrameworkElement>();if(f && f.Name()==L"QuickAudioPanel")return;}
                    auto view=std::make_shared<View>();try{view->Attach(footer);}catch(...){view->Detach();throw;}
                    {std::lock_guard<std::mutex> lock(viewsLock);views.push_back(view);}
                    auto bus=std::atomic_load(&sharedBus);if(bus){auto state=std::make_shared<qa::State>();if(bus->Snapshot(*state))view->Apply(state);}
                }catch(...){Log(L"INSERT_ERROR "+Error());}
            });
        }catch(...){Log(L"CALLBACK_ERROR "+Error());}return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnElementStateChanged(InstanceHandle,VisualElementState,LPCWSTR) noexcept override{return S_OK;}
};
struct Tap:winrt::implements<Tap,IObjectWithSite,winrt::non_agile>{
    winrt::com_ptr<IUnknown> site;winrt::com_ptr<Watcher> watcher;
    HRESULT STDMETHODCALLTYPE SetSite(IUnknown* s) noexcept override{try{site.copy_from(s);if(site){stopping=false;Log(L"XAML_SET_SITE");std::thread(Listen).detach();watcher=winrt::make_self<Watcher>(site);watcher->Start();}return S_OK;}catch(...){Log(L"SITE_ERROR "+Error());return winrt::to_hresult();}}
    HRESULT STDMETHODCALLTYPE GetSite(REFIID iid,void** p) noexcept override{return site.as(iid,p);}
};
struct Factory:winrt::implements<Factory,IClassFactory,winrt::non_agile>{
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer,REFIID iid,void** p) noexcept override{if(outer)return CLASS_E_NOAGGREGATION;try{return winrt::make<Tap>().as(iid,p);}catch(...){return winrt::to_hresult();}}
    HRESULT STDMETHODCALLTYPE LockServer(BOOL) noexcept override{return S_OK;}
};
STDAPI DllGetClassObject(REFCLSID cls,REFIID iid,void** p){if(cls!=qa::TapClsid)return CLASS_E_CLASSNOTAVAILABLE;try{return winrt::make<Factory>().as(iid,p);}catch(...){return winrt::to_hresult();}}
STDAPI DllCanUnloadNow(){return S_FALSE;}
BOOL WINAPI DllMain(HMODULE module,DWORD why,LPVOID){if(why==DLL_PROCESS_ATTACH){moduleHandle=module;DisableThreadLibraryCalls(module);}return TRUE;}
