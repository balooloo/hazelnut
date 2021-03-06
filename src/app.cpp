/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * hazelnut
 * Copyright (C) Emilien Kia 2012 <emilien.kia@gmail.com>
 * 
hazelnut is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * hazelnut is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <wx/wx.h>

#include <wx/artprov.h>
#include <wx/aboutdlg.h>

#include "app.hpp"

#include "config-dialog.hpp"
#include "task-bar-icon.hpp"

#include "../data/icons/16x16/status/battery-missing.xpm"
#include "../data/icons/16x16/status/battery-empty.xpm"
#include "../data/icons/16x16/status/battery-caution.xpm"
#include "../data/icons/16x16/status/battery-caution-charging.xpm"
#include "../data/icons/16x16/status/battery-low.xpm"
#include "../data/icons/16x16/status/battery-low-charging.xpm"
#include "../data/icons/16x16/status/battery-good.xpm"
#include "../data/icons/16x16/status/battery-good-charging.xpm"
#include "../data/icons/16x16/status/battery-full.xpm"
#include "../data/icons/16x16/status/battery-full-charging.xpm"

// ----------------------------------------------------------------------------
// HazelnutApp implementation
// ----------------------------------------------------------------------------
IMPLEMENT_APP(HazelnutApp)

BEGIN_EVENT_TABLE(HazelnutApp, wxApp)
	EVT_TIMER(wxID_ANY, HazelnutApp::OnTimerUpdate)
END_EVENT_TABLE()

bool HazelnutApp::OnInit()
{
	connected = false;
	config = NULL;
	
	// Create the timer for updatting status
	timer = new wxTimer(this);
	// Create task bar icon
    taskBarIcon = new HazelnutTaskBarIcon();

	// Intend to connect to UPSD
	if(TryConnect())
	{
		// Intend to select power source (from conf or from available source list)
		AutoselectPowerSource();
	}
	
    dialog = new HazelnutConfigDialog(wxT("Hazelnut"));
    dialog->Show(false);
    SetTopWindow(dialog);

    return true;
}

bool HazelnutApp::TryConnect()
{
	connected = upscli_connect(&upsconn, "localhost", 3493, 0) == 0;
	return connected;
}

bool HazelnutApp::AutoselectPowerSource()
{
	wxConfig* conf = GetConfig();
	if(conf->HasEntry(wxT("dev")))
	{
		wxString dev = conf->Read(wxT("dev"), wxT(""));
		if(dev.IsEmpty())
		{
			SetNoDevice();
		}
		else
		{
			SetDevice(Device(dev, &upsconn));
		}
		return true;
	}
	else
	{
		std::list<Device> list = Device::getUps(&upsconn);
		if(list.empty())
		{
			SetNoDevice();
			return false;
		}
		else
		{
			SetDevice(list.front());
			return true;
		}
	}
}

void HazelnutApp::SetDevice(const Device& dev)
{
wxLogDebug(wxT("HazelnutApp::SetDevice : %s\n"), (const char*)dev.getId().c_str());
	if(timer->IsRunning())
		timer->Stop();

	device = dev;
	Update();

	if(device.IsOk())
	{
		GetConfig()->Write(wxT("dev"), dev.getId());
		timer->Start(5*1000);
	}
	else
	{
		GetConfig()->Write(wxT("dev"), wxT(""));
	}
}

void HazelnutApp::SetNoDevice()
{
	SetDevice(Device());
}

void HazelnutApp::Update()
{
	if(!device.IsOk())
	{
		taskBarIcon->SetIcon(wxIcon(battery_missing_16), wxT("No device selected"));
	}
	else
	{
		wxString vendor = device.getVar("device.mfr"),
				 model  = device.getVar("device.model"),
				 label = vendor + wxT(" - ") + model,
				 batteryCharge = device.getVar("battery.charge");
		
		double batCharge = -1;
		batteryCharge.ToDouble(&batCharge);
		
		if(batCharge < 0 || batCharge > 100)
		{
			taskBarIcon->SetIcon(wxIcon(battery_missing_16), label + wxT("\nError in battery charge reading"));
		}
		else
		{
			wxString msg = label + wxT("\nBattery charge: ") + batteryCharge + wxT("%");
			if(batCharge < 10)
			{
				taskBarIcon->SetIcon(wxIcon(battery_empty_16), msg );
			}
			else if(batCharge < 30)
			{
				taskBarIcon->SetIcon(wxIcon(battery_low_16), msg);
			}
			else if(batCharge < 90)
			{
				taskBarIcon->SetIcon(wxIcon(battery_good_16), msg);
			}
			else
			{
				taskBarIcon->SetIcon(wxIcon(battery_full_16), msg);
			}
		}
	}
}

std::list<Device> HazelnutApp::getUps()
{
	return Device::getUps(&upsconn);
}

wxConfig* HazelnutApp::GetConfig()
{
	if(config==NULL)
		config = new wxConfig(wxT("hazelnut"));
	return config;
}

void HazelnutApp::About()
{
    wxAboutDialogInfo info;
    info.SetName(_("Hazelnut"));
    info.SetVersion(_("0.1 Alpha"));
    info.SetDescription(_("This program does something great."));
    info.SetCopyright(_T("(C) 2012 Emilien Kia <emilien.kia@free.fr>"));
    wxAboutBox(info);
}

void HazelnutApp::ShowDialog()
{
    dialog->Show();
}
    
void HazelnutApp::Exit()
{
    if(taskBarIcon)
    {
        taskBarIcon->RemoveIcon();
        delete taskBarIcon;
        taskBarIcon = NULL;
    }
    dialog->Hide();
    dialog->Close();
    dialog->Destroy();

	if(config)
	{
		delete config;
		config = NULL;
	}
}

void HazelnutApp::OnTimerUpdate(wxTimerEvent& event)
{
	Update();
}
