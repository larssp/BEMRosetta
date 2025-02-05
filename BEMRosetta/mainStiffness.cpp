// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2020 - 2022, the BEMRosetta author and contributors
#include <CtrlLib/CtrlLib.h>
#include <Controls4U/Controls4U.h>
#include <ScatterCtrl/ScatterCtrl.h>
#include <GLCanvas/GLCanvas.h>
#include <RasterPlayer/RasterPlayer.h>
#include <TabBar/TabBar.h>
#include <DropGrid/DropGrid.h>

#include <BEMRosetta_cl/BEMRosetta.h>

using namespace Upp;

#include "main.h"

using namespace Eigen;


void MainMatrixKA::Init(Hydro::DataMatrix what) {
	CtrlLayout(*this);
	
	Ndim = false;
	this->what = what;
	
	numDigits <<= THISBACK(PrintData);
	numDecimals <<= THISBACK(PrintData);
	opEmptyZero <<= THISBACK(PrintData);
	opUnits <<= THISBACK(PrintData);
	expRatio <<= THISBACK(PrintData);
	if (IsNull(expRatio)) 
		expRatio <<= 3;
	
	opDecimals <<= THISBACK1(OnOp, 0);
	opDigits <<= THISBACK1(OnOp, 1);
	
	OnOp(~opDigits ? 0 : 1);
}

void MainMatrixKA::OnOp(int id) {
	if (id == 0) 
		opDigits <<= 1;
	else 
		opDecimals <<= 1;
	
	numDigits.Enable(id == 1);
	numDecimals.Enable(id == 0);
	
	PrintData();
}
	
void MainMatrixKA::Clear() {
	array.Reset();
	array.NoHeader().SetLineCy(EditField::GetStdHeight()).HeaderObject().Absolute();
	array.MultiSelect().SpanWideCells();
	array.WhenBar = [&](Bar &menu) {ArrayCtrlWhenBar(menu, array);};
	
	data.Clear();
	row0s.Clear();
	col0s.Clear();
}

void MainMatrixKA::AddPrepare(int &row0, int &col0, String name, int icase, String bodyName, int ibody, int idc, int ncol) {
	row0 = ibody*9 + 1;
	int realicase = 0;
	for (int i = 0; i < icase; ++i) 
		realicase += int(data[realicase].cols())/6;
	col0 = realicase*7 + icase;
	
	row0s << row0;
	col0s << col0;
	
	array.Set(0, col0, AttrText(name).Bold());
	array.Set(2, col0, AttrText(" ").Paper(GetColorId(idc)));
	
	int len = StdFont().Bold().GetAveWidth()*(BEM::StrDOF_len() + 2);
	int ncols = int(data[realicase].cols());
	int Nb = int(ncols/6);
	while (array.GetColumnCount() < col0 + 1 + ncols) {
		if (icase > 0) {
			array.AddColumn("", max(10, len));
			int icol = array.GetColumnCount() - 1;
			array.HeaderObject().Tab(icol).SetMargin(0);
		}
		array.AddColumn("", max(20, len));
		
		for (int nmat = 0; nmat < Nb; ++nmat) {
			for (int i = 0; i < 2; ++i) 
				array.AddColumn("", what == Hydro::MAT_K ? max(20, len) : max(70, len));	
			for (int i = 2; i < 6; ++i)
				array.AddColumn("", max(70, len));
			if (Nb > 1 && nmat < Nb-1) 
				array.AddColumn("", 10);
		}
	}
	if (Nb == 1)
		array.Set(row0, col0 + 1, AttrText(Format(t_("#%d body %s"), ibody + 1, bodyName)).Bold());
	else {
		for (int nmat = 0; nmat < Nb; ++nmat) 
			array.Set(row0, col0 + nmat*7 + 1, AttrText(Format(t_("#%d-%d body %s"), ibody + 1, nmat + 1, bodyName)).Bold());
	}
	
	for (int i = 0; i < 6; ++i) 
		array.Set(row0 + i + 2, col0, 	  	  AttrText(BEM::StrDOF(i)).Bold().Align(ALIGN_CENTER));
	for (int nmat = 0; nmat < Nb; ++nmat) {
		for (int i = 0; i < 6; ++i) 
			array.Set(row0 + 1, 	col0 + i + 1 + nmat*7, AttrText(BEM::StrDOF(i)).Bold().Align(ALIGN_CENTER));
	}
	for (int i = 1; i <= ibody; ++i) 
		array.SetLineCy(i*9, 10);	
}

double MaxIsNum(const double *data, int sz) {
	double mx = NaNDouble;
	for (int i = 0; i < sz; ++i) {
		if (IsNum(data[i]) && (!IsNum(mx) || data[i] > mx))
			mx = data[i];
	}
	return mx;
}

void MainMatrixKA::PrintData() {
	expRatio.Enable(~opEmptyZero);
	double exr = double(~expRatio);
	double ratio = 1/pow(10., exr);
	
	if (IsNull(numDigits) || numDigits < 5 || numDigits > 14)
		numDigits <<= 5;
	if (IsNull(numDecimals) || numDecimals < 0 || numDecimals > 14)
		numDecimals <<= 0;
	for (int i = 0; i < data.size(); ++i) {
		int row0 = row0s[i];
		int col0 = col0s[i];
		if (data[i].size() != 0) {
			double mx = MaxIsNum(data[i].data(), int(data[i].size()));
			if (IsNum(mx)) {
				for (int r = 0; r < data[i].rows(); ++r) {
					for (int c = 0; c < data[i].cols(); ++c) {
						String sdata;
						if (~opUnits) {
							if (what == Hydro::MAT_K)
								sdata = Hydro::K_units(Ndim, r, c);
							else if (what == Hydro::MAT_A) 
								sdata = Hydro::A_units(Ndim, r, c);
							else if (what == Hydro::MAT_M)
								sdata = Hydro::M_units(r, c);
							else if (what == Hydro::MAT_DAMP_LIN)
								sdata = Hydro::B_units(Ndim, r, c);
						} else {
							double val = data[i](r, c);
							if (!IsNum(val)) 
								sdata = "-";
							else {
								double rat = mx == 0 ? 0 : abs(val/mx);
								if (~opEmptyZero && rat < ratio)
									sdata = "";
								else if (bool(~opDigits) == 0)
									sdata = FDS(val, numDigits);
								else
									sdata = FormatF(val, numDecimals);
							}
						}
						array.Set(row0 + r + 2, col0 + c + 1 + int(c/6), AttrText(sdata).Align(ALIGN_RIGHT));
					}
				}
			}
		}
	}
}

void MainMatrixKA::Add(const Mesh &mesh, int icase, bool button) {
	String name = mesh.fileName;
	
	const MatrixXd &K = mesh.C;
	data << clone(K);
	
	int idc = mesh.GetId();
	
	int row0, col0;
	AddPrepare(row0, col0, name, icase, "", 0, idc, 6);
	
	if (K.size() == 0)
		return;

	if (button) {
		array.CreateCtrl<Button>(row0, col0+5, false).SetLabel(t_("Save")).Tip(t_("Saves to Wamit .hst or Nemoh stiffness matrix format"))
			<< [&] {
				FileSel fs;
				fs.NoExeIcons();
				fs.ActiveDir(hstFolder);
				fs.Type(t_("Wamit stiffness matrix format"), "*.hst");
				fs.Type(t_("Nemoh stiffness matrix format"), "KH.dat");
				if (fs.ExecuteSaveAs(t_("Save to stiffness matrix format"))) {
					String ext = GetFileExt(~fs);
					if (ext == ".hst")
						static_cast<const WamitMesh &>(mesh).SaveHST(~fs, Bem().rho, Bem().g);
					else if (ext == ".dat")
						static_cast<const NemohMesh &>(mesh).SaveKH(~fs);
					else
						throw Exc(t_("Unknown file format"));
				}
				hstFolder = GetFileFolder(~fs);
			};
	}
	if (button && Bem().hydros.size() > 0) {
		array.CreateCtrl<Button>(row0, col0+6, false).SetLabel(t_("Copy")).Tip(t_("Copies matrix and paste it in selected BEM Coefficients file and body"))
			<< [&] {
				WithBEMList<TopWindow> w;
				CtrlLayout(w);
				w.Title(t_("Copies matrix and paste it in selected BEM Coefficients file and body"));
				w.array.SetLineCy(EditField::GetStdHeight());
				w.array.AddColumn(t_("File"), 20);
				w.array.AddColumn(t_("Body"), 10);
				w.array.HeaderObject().HideTab(w.array.AddColumn().HeaderTab().GetIndex());
				w.array.HeaderObject().HideTab(w.array.AddColumn().HeaderTab().GetIndex());
				for (int f = 0; f < Bem().hydros.size(); ++f) {
					const Hydro &hy = Bem().hydros[f].hd();
					for (int ib = 0; ib < hy.Nb; ++ib)
						w.array.Add(hy.name, hy.names[ib].IsEmpty() ? AsString(ib+1) : hy.names[ib], f, ib);
				}
				bool cancel = true;
				w.butSelect << [&] {cancel = false;	w.Close();};
				w.butCancel << [&] {w.Close();}; 
				w.Execute();
				if (!cancel) {
					int id = w.array.GetCursor();
					if (id < 0)
						return; 
					
					int hid = w.array.Get(id, 2);
					Hydro &hydro = Bem().hydros[hid].hd();
					
					int ib = w.array.Get(id, 3);
					
					VectorXd c0 = hydro.c0.col(ib);
					if (Distance(mesh.c0, Point3D(c0[0], c0[1], c0[2])) > 0.1)
						if (!ErrorOKCancel(Format(t_("Centre of rotation in mesh (%.1f,%.1f,%.1f) and in bem model (%.1f,%.1f,%.1f) are different.&Do you wish to continue?"), 
														mesh.c0.x, mesh.c0.y, mesh.c0.z, c0[0], c0[1], c0[2])))
							cancel = true;	
					VectorXd cg = hydro.cg.col(ib);
					if (!cancel && Distance(mesh.cg, Point3D(cg[0], cg[1], cg[2])) > 0.1)
						if (!ErrorOKCancel(Format(t_("Centre of gravity in mesh (%.1f,%.1f,%.1f) and in bem model (%.1f,%.1f,%.1f) are different.&Do you wish to continue?"), 
														mesh.c0.x, mesh.c0.y, mesh.c0.z, c0[0], c0[1], c0[2])))
							cancel = true;
					if (!cancel)
						hydro.SetC(ib, K);
				}
			 };
	}
}
	
void MainMatrixKA::Add(String name, int icase, String bodyName, int ib, const Hydro &hydro, int idc, bool ndim) {
	if (what == Hydro::MAT_K) {
		if (!hydro.IsLoadedC())
			data << EigenNull;
		else
			data << hydro.C_(ndim, ib);
		label.SetText(Format(t_("Hydrostatic Stiffness Matrices (%s)"),
						ndim ? t_("'dimensionless'") : t_("dimensional")));
	} else if (what == Hydro::MAT_A) {
		if (!hydro.IsLoadedA())
			data << EigenNull;
		else {
			if (Bem().onlyDiagonal)
				data << hydro.Ainf_mat(ndim, ib, ib);
			else {
				MatrixXd mat(6, 6*hydro.Nb);
				for (int ib2 = 0; ib2 < hydro.Nb; ++ib2)
					mat.block(0, ib2*6, 6, 6) = hydro.Ainf_mat(ndim, ib, ib2);
				data << mat;
			}
		}
		label.SetText(Format(t_("Added Mass at infinite frequency (%s)"), 
						ndim ? t_("'dimensionless'") : t_("dimensional")));
	} else if (what == Hydro::MAT_M) {
		if (!hydro.IsLoadedM())
			data << EigenNull;
		else
			data << hydro.M[ib];
		label.SetText(t_("Mass/Inertia matrix (dimensional)"));
	} else if (what == Hydro::MAT_DAMP_LIN) {
		if (!hydro.IsLoadedDlin())
			data << EigenNull;
		else
			data << hydro.Dlin_dim(ib);
		label.SetText(t_("Additional linear damping (dimensional)"));
	} else
		NEVER();

	int row0, col0;
	AddPrepare(row0, col0, name, icase, bodyName, ib, idc, Bem().onlyDiagonal && what == Hydro::MAT_A ? 6*hydro.Nb : 6);
}

bool MainMatrixKA::Load(UArray<HydroClass> &hydros, const UVector<int> &ids, bool ndim) {
	Clear();
	Ndim = ndim;
	
	bool loaded = false; 	
	for (int i = 0; i < ids.size(); ++i) {
		int isurf = ids[i];
		Hydro &hydro = hydros[isurf].hd();
		for (int ib = 0; ib < hydro.Nb; ++ib) 
			Add(hydro.name, i, hydro.names[ib], ib, hydro, hydro.GetId(), ndim);
		if (data.size() > i && data[i].size() > 0 && IsNum(data[i](0))) 
			loaded = true;
	}
	if (!loaded)	
		return false;
	
	PrintData();
	return true;
}	

void MainMatrixKA::Load(UArray<Mesh> &surfs, const UVector<int> &ids) {
	Clear();
	Ndim = false;
	
	for (int i = 0; i < ids.size(); ++i) {
		int isurf = ids[i];
		if (isurf >= 0)	
			Add(surfs[isurf], i, true);
	}
	PrintData();
}
