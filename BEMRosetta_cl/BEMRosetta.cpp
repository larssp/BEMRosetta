// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2020 - 2022, the BEMRosetta author and contributors
#include "BEMRosetta.h"
#include "BEMRosetta_int.h"
#include <ScatterDraw/DataSource.h>
#include <ScatterDraw/Equation.h>
#include "functions.h"

#include <MatIO/matio.h>

using namespace Upp;
using namespace Eigen;

#ifdef flagDEBUG
#include <SysInfo/Crash.h>
static CrashHandler crash;
#endif


Function <void(String)> BEM::Print 		  = [](String s) {Cout() << s;};
Function <void(String)> BEM::PrintWarning = [](String s) {Cout() << s;};
Function <void(String)> BEM::PrintError   = [](String s) {Cout() << s;};

const char *BEM::strDOFtext[] 	 = {t_("surge"), t_("sway"), t_("heave"), t_("roll"), t_("pitch"), t_("yaw")};
const char *BEM::strDOFtextAbrev[] = {t_("s"), t_("w"), t_("h"), t_("r"), t_("p"), t_("y")};
const char *BEM::strDOFnum[] 	 = {t_("1"), t_("2"), t_("3"), t_("4"), t_("5"), t_("6")};
const char *BEM::strDOFxyz[] 	 = {t_("x"), t_("y"), t_("z"), t_("rx"), t_("ry"), t_("rz")};

const char *Hydro::strDataToPlot[] = {t_("A(ω)"), t_("A∞"), t_("A0"), t_("B(ω)"), t_("A∞(ω)"), t_("Kirf"),
				t_("Fsc_ma"), t_("Fsc_ph"), t_("Ffk_ma"), t_("Ffk_ph"), t_("Fex_ma"), t_("Fex_ph"),
				t_("RAO_ma"), t_("RAO_ph"), t_("Z_ma"), t_("Z_ph"), t_("Kr_ma"), t_("Kr_ph"), 
				t_("TFS_ma"), t_("TFS_ph")};

const char *BEM::strDOFType[] = {t_("1,2,3,4,5,6"), t_("surge,sway,"), t_("x,y,z,rx,ry,rz"), ""};
BEM::DOFType BEM::dofType = BEM::DOFSurgeSway;

const char *BEM::strHeadingType[] = {t_("-180->180º"), t_("0->360º"), ""};
BEM::HeadingType BEM::headingType = BEM::HEAD_180_180;
	
const char *BEMCase::solverStr[] = {t_("Nemoh"), t_("Nemoh v115"), t_("Nemoh v3"), t_("Capytaine"), t_("HAMS"), t_("AQWA")};

int Hydro::idCount = 0;	

bool PrintStatus(String s, int) {
	Cout() << "\n" << RemoveAccents(s);
	return true;
};

void Hydro::Initialize_Sts() {
	sts.SetCount(6*Nb);
	for (int ib = 0; ib < 6*Nb; ++ib) 
		sts[ib].SetCount(6*Nb);
}

void Hydro::GetFexFromFscFfk() {
	for (int ih = 0; ih < Nh; ++ih) {
		for (int ifr = 0; ifr < Nf; ++ifr) {
			for (int i = 0; i < Nb*6; ++i) {
				if (IsNum(sc.force[ih](ifr, i))) 
					ex.force[ih](ifr, i) = sc.force[ih](ifr, i) + fk.force[ih](ifr, i);
			}
		}
	}
}

void Hydro::Normalize() {
	if (IsLoadedC()) {
		for (int ib = 0; ib < Nb; ++ib) {
			for (int idf = 0; idf < 6; ++idf) 
				for (int jdf = 0; jdf < 6; ++jdf) 
					C[ib](idf, jdf) = C_ndim(ib, idf, jdf);
		}
	}
	if (IsLoadedA() && IsLoadedB()) {
		for (int ifr = 0; ifr < Nf; ++ifr) {
			for (int idf = 0; idf < 6*Nb; ++idf) {
				for (int jdf = 0; jdf < 6*Nb; ++jdf) {	
					A[idf][jdf][ifr] = A_ndim(ifr, idf, jdf);
					B[idf][jdf][ifr] = B_ndim(ifr, idf, jdf);
				}
			}
		}
	}
	if (IsLoadedAinf()) {
		for (int i = 0; i < 6*Nb; ++i) 
			for (int j = 0; j < 6*Nb; ++j) 
				Ainf(i, j) = Ainf_ndim(i, j);
	}
	if (IsLoadedA0()) {
		for (int i = 0; i < 6*Nb; ++i) 
			for (int j = 0; j < 6*Nb; ++j) 
				A0(i, j) = A0_ndim(i, j);
	}
	if (IsLoadedFex())
    	Normalize_Forces(ex);
	if (IsLoadedFsc())
		Normalize_Forces(sc);
	if (IsLoadedFfk())
		Normalize_Forces(fk);
	if (IsLoadedRAO()) {
		for (int h = 0; h < Nh; ++h) 
			for (int ifr = 0; ifr < Nf; ++ifr) 
				for (int i = 0; i < 6*Nb; ++i) 	 
					rao.force[h](ifr, i) = R(h, ifr, i);
	}
}

void Hydro::Dimensionalize() {
	if (IsLoadedC()) {
		for (int ib = 0; ib < Nb; ++ib) {
			for (int idf = 0; idf < 6; ++idf) 
				for (int jdf = 0; jdf < 6; ++jdf) 
					C[ib](idf, jdf) = C_dim(ib, idf, jdf);
		}
	}
	if (IsLoadedA() && IsLoadedB()) {
		for (int ifr = 0; ifr < Nf; ++ifr) {
			for (int idf = 0; idf < 6*Nb; ++idf) {
				for (int jdf = 0; jdf < 6*Nb; ++jdf) {	
					A[idf][jdf][ifr] = A_dim(ifr, idf, jdf);
					B[idf][jdf][ifr] = B_dim(ifr, idf, jdf);
				}
			}
		}
	}
	if (IsLoadedAinf()) {	
		for (int i = 0; i < 6*Nb; ++i) 
			for (int j = 0; j < 6*Nb; ++j) 
				Ainf(i, j) = Ainf_dim(i, j);
	}
	if (IsLoadedA0()) {	
		for (int i = 0; i < 6*Nb; ++i) 
			for (int j = 0; j < 6*Nb; ++j) 
				A0(i, j) = A0_ndim(i, j);
	}
	if (IsLoadedFex())
    	Dimensionalize_Forces(ex);
	if (IsLoadedFsc())
		Dimensionalize_Forces(sc);
	if (IsLoadedFfk())
		Dimensionalize_Forces(fk);
	if (IsLoadedRAO()) {
		for (int h = 0; h < Nh; ++h) 
			for (int ifr = 0; ifr < Nf; ++ifr) 
				for (int i = 0; i < 6*Nb; ++i) 
					rao.force[h](ifr, i) = R(h, ifr, i);
	}
}

void Hydro::Initialize_AB(UArray<UArray<VectorXd>> &a) {
	a.SetCount(6*Nb);
	for (int i = 0; i < 6*Nb; ++i) {
		a[i].SetCount(6*Nb);
		for (int j = 0; j < 6*Nb; ++j) 
			a[i][j].setConstant(Nf, NaNDouble);
	}
}

void Hydro::Initialize_Forces() {
	Initialize_Forces(ex);
	Initialize_Forces(sc);
	Initialize_Forces(fk);
}

void Hydro::Initialize_Forces(Forces &f, int _Nh) {
	if (_Nh == -1)
		_Nh = Nh;
	f.force.SetCount(_Nh);
	for (int ih = 0; ih < _Nh; ++ih) 
		f.force[ih].setConstant(Nf, Nb*6, NaNComplex);
}
	
void Hydro::Normalize_Forces(Forces &f) {
	for (int ih = 0; ih < Nh; ++ih) 
		for (int ifr = 0; ifr < Nf; ++ifr) 
			for (int idf = 0; idf < 6*Nb; ++idf) 
				f.force[ih](ifr, idf) = F_dim(f, ih, ifr, idf);
}

void Hydro::Dimensionalize_Forces(Forces &f) {
	for (int ih = 0; ih < Nh; ++ih) 
		for (int ifr = 0; ifr < Nf; ++ifr) 
			for (int idf = 0; idf < 6*Nb; ++idf) 
				f.force[ih](ifr, idf) = F_dim(f, ih, ifr, idf);
}

void Hydro::Add_Forces(Forces &to, const Hydro &hydro, const Forces &from) {
	if (hydro.IsLoadedForce(from)) {
		for (int ihhy = 0; ihhy < hydro.Nh; ++ihhy) {
			int ih = FindClosest(head, hydro.head[ihhy]);
			for (int ifrhy = 0; ifrhy < hydro.Nf; ++ifrhy) {
				int ifr = FindClosest(w, hydro.w[ifrhy]);
				for (int idf = 0; idf < 6*Nb; ++idf) 	 
					if (IsNum(from.force[ihhy](ifrhy, idf))) 
						to.force[ih](ifr, idf) = hydro.F_ndim(from, ihhy, ifrhy, idf);
			}
		} 
	}
}

void Hydro::Symmetrize_Forces_Each0(const Forces &f, Forces &newf, const UVector<double> &newHead, double h, int ih, int idb) {
	int nih  = FindClosest(newHead, h);
	bool avg  = IsNum(newf.force[nih](0, idb));
	for (int ifr = 0; ifr < Nf; ++ifr) {
		if (avg) 
			newf.force[nih](ifr, idb) = Avg(newf.force[nih](ifr, idb), f.force[ih](ifr, idb));
		else 
			newf.force[nih](ifr, idb) = f.force[ih](ifr, idb);
	}
}

static double MirrorHead(double head, bool xAxis) {
	if (xAxis)
		return -head;
	else {
		head = FixHeading_180(head);
		head += 90;
		head = FixHeading_180(-head);
		head -= 90;
		return head;		
	}
}

void Hydro::Symmetrize_ForcesEach(const Forces &f, Forces &newf, const UVector<double> &newHead, int newNh, bool xAxis) {
	Initialize_Forces(newf, newNh);
	
	for (int idb = 0; idb < 6*Nb; ++idb) {
		for (int ih = 0; ih < Nh; ++ih) {
			Symmetrize_Forces_Each0(f, newf, newHead, FixHeading_180(head[ih]), ih, idb);
			Symmetrize_Forces_Each0(f, newf, newHead, FixHeading_180(MirrorHead(head[ih], xAxis)), ih, idb);
		}
	}
}

void Hydro::Copy(const Hydro &hyd) {
	file = hyd.file;
	name = hyd.name;
	g = hyd.g;
    h = hyd.h;
    rho = hyd.rho;
    len = hyd.len;
    dimen = hyd.dimen;
    Nb = hyd.Nb;
    Nf = hyd.Nf;
    Nh = hyd.Nh;

    A = clone(hyd.A);
	Ainf_w = clone(hyd.Ainf_w);
    Ainf = clone(hyd.Ainf);
    A0 = clone(hyd.A0);
	
	Dlin = clone(Dlin);
	Cmoor = clone(Cmoor);
	
    B = clone(hyd.B);
    
    head = clone(hyd.head);
    names = clone(hyd.names);
    C = clone(hyd.C);
    M = clone(hyd.M);
    cb = clone(hyd.cb);
    cg = clone(hyd.cg);
    c0 = clone(hyd.c0);
    code = hyd.code;     
    dof = clone(hyd.dof); 
    
    Kirf = clone(hyd.Kirf);
    Tirf = clone(hyd.Tirf);
    
    ex = clone(hyd.ex);
    sc = clone(hyd.sc);
    fk = clone(hyd.fk);
    rao = clone(hyd.rao);
    
    description = hyd.description;

    sts = clone(hyd.sts);
    dimenSTS = hyd.dimenSTS;
    stsProcessor = hyd.stsProcessor;
    
    qtfsum = clone(hyd.qtfsum);
    qtfdif = clone(hyd.qtfdif);
    qw = clone(hyd.qw);
    qh = clone(hyd.qh);
    qtfdataFromW = hyd.qtfdataFromW;
    
    mdhead = clone(mdhead);
	md = clone(md);
	mdtype = hyd.mdtype;
	    
    T = clone(hyd.T);
    w = clone(hyd.w);
    dataFromW = hyd.dataFromW;
    Vo = clone(hyd.Vo); 
    
    bem = hyd.bem;
}

void AvgB(Eigen::MatrixXd &ret, const UArray<const Eigen::MatrixXd*> &d) {
	int numT = d.size();
	if (numT == 0) 
		return;
	
	int num = int(d[0]->size());
	for (int it = 1; it < numT; ++it) 
		if (d[it]->size() != num)
			throw Exc(t_("Avg() has to have same number of values"));
	
	for (int i = 0; i < num; ++i) {
		Eigen::VectorXd r(numT);
		for (int it = 0; it < numT; ++it) 
			r[it] = (*d[it])(i);
		ret(i) = r.mean();
	}
}

using UA = UArray<UArray<VectorXd>>;
void AvgB(UA &ret, const UArray<const UA*> &d) {
	int numT = d.size();
	if (numT == 0) 
		return;

	for (int i = 0; i < ret.size(); ++i) {
		for (int j = 0; j < ret[i].size(); ++j) {
			for (int k = 0; k < ret[i][j].size(); ++k) {
				Eigen::VectorXd r(numT);
				for (int it = 0; it < numT; ++it) 
					r[it] = (*d[it])[i][j][k];
				ret[i][j][k] = r.mean();
			}
		}
	}
}

using UAM = UArray<MatrixXd>;
void AvgB(UAM &ret, const UArray<const UAM*> &d) {
	int numT = d.size();
	if (numT == 0) 
		return;

	for (int i = 0; i < ret.size(); ++i) {
		for (int j = 0; j < ret[i].size(); ++j) {
			Eigen::VectorXd r(numT);
			for (int it = 0; it < numT; ++it) 
				r[it] = (*d[it])[i](j);
			ret[i](j) = r.mean();
		}
	}
}

void AvgB(Hydro::Forces &ret, const UArray<const Hydro::Forces*> &d) {
	int numT = d.size();
	if (numT == 0) 
		return;

	for (int i = 0; i < ret.force.size(); ++i) {
		for (int j = 0; j < ret.force[i].cols(); ++j) {
			for (int k = 0; k < ret.force[i].rows(); ++k) {
				Eigen::VectorXcd r(numT);
				for (int it = 0; it < numT; ++it) 
					r[it] = (*d[it]).force[i](k, j);
				ret.force[i](k, j) = r.mean();
			}
		}
	}
}

using UMD = UArray<UArray<UArray<VectorXd>>>;
void AvgB(UMD &ret, const UArray<const UMD*> &d) {
	int numT = d.size();
	if (numT == 0) 
		return;

	for (int i = 0; i < ret.size(); ++i) {
		for (int j = 0; j < ret[i].size(); ++j) {
			for (int k = 0; k < ret[i][j].size(); ++k) {
				for (int l = 0; l < ret[i][j][k].size(); ++l) {
					Eigen::VectorXd r(numT);
					for (int it = 0; it < numT; ++it) 
						r[it] = (*d[it])[i][j][k][l];
					ret[i][j][k][l] = r.mean();
				}
			}
		}
	}
}

using UMQ = UArray<UArray<UArray<MatrixXcd>>>;
void AvgB(UMQ &ret, const UArray<const UMQ*> &d) {
	int numT = d.size();
	if (numT == 0) 
		return;

	for (int i = 0; i < ret.size(); ++i) {
		for (int j = 0; j < ret[i].size(); ++j) {
			for (int k = 0; k < ret[i][j].size(); ++k) {
				for (int l = 0; l < ret[i][j][k].rows(); ++l) {
					for (int m = 0; l < ret[i][j][k].cols(); ++m) {
						Eigen::VectorXcd r(numT);
						for (int it = 0; it < numT; ++it) 
							r[it] = (*d[it])[i][j][k](l, m);
						ret[i][j][k](l, m) = r.mean();
					}
				}
			}
		}
	}
}

void Hydro::Average(const UArray<HydroClass> &hydros, const UVector<int> &ids) {
	const Hydro &h0 = hydros[0].hd();
	Nb = h0.Nb;
    Nf = h0.Nf;
    Nh = h0.Nh;
	h = h0.h;
	rho = h0.rho;
	g = h0.g;

	T = clone(h0.T);
    w = clone(h0.w);
    head = clone(h0.head);
	qw = clone(h0.qw);
    qh = clone(h0.qh);
	mdhead = clone(h0.mdhead);
	c0 = clone(h0.c0);
	dof = clone(h0.dof);
	
	for (int i = 1; i < ids.size(); ++i) {
		const Hydro &hy = hydros[i].hd();
		if (Nb != hy.Nb)
			throw Exc(t_("All models have to have the same number of bodies"));
		if (Nf != hy.Nf)
			throw Exc(t_("All models have to have the same number of frequencies"));
		if (Nh != hy.Nh)
			throw Exc(t_("All models have to have the same number of headings"));
		if (rho != hy.rho)
			throw Exc(t_("All models have to have the same density"));
		if (g != hy.g)
			throw Exc(t_("All models have to have the same gravity"));
		if (h != hy.h) 
			h = -1;
		if (!CompareDecimals(T, hy.T, 2))
			throw Exc(t_("All models have to have the same periods"));
		if (!CompareDecimals(w, hy.w, 2))
			throw Exc(t_("All models have to have the same frequencies"));
		if (!CompareDecimals(head, hy.head, 2))
			throw Exc(t_("All models have to have the same headings"));
		if (!CompareDecimals(qw, hy.qw, 2))
			throw Exc(t_("All models have to have the same frequencies in QTF"));
		if (!Compare(qh, hy.qh))
			throw Exc(t_("All models have to have the same headings in QTF"));
		if (!Compare(mdhead, hy.mdhead))
			throw Exc(t_("All models have to have the same headings in mean drift"));
		if (c0 != hy.c0)
			throw Exc(t_("All models have to have the same centre of reference"));
		if (dof != hy.dof)
			throw Exc(t_("All models have to have the same number of dof"));
	}
	
	bem = h0.bem;
	
	file = "Average";
	name = "Average";
	description = "Average";
	code = BEMROSETTA;
	
    len = 1;
    dimen = true;
    
    dataFromW = qtfdataFromW = true;
    
    names = clone(h0.names);
    
    mdtype = h0.mdtype;
    
    Vo.SetCount(Nb, NaNDouble);
	cg.setConstant(3, Nb, NaNDouble);
	cb.setConstant(3, Nb, NaNDouble);
	Ainf.setConstant(Nb*6, Nb*6, NaNDouble);
	A0.setConstant(Nb*6, Nb*6, NaNDouble);
	
	Initialize_AB(A);
	Initialize_AB(B);
	
	C.SetCount(Nb);
	for (int ib = 0; ib < Nb; ++ib) 
		C[ib].setConstant(6, 6, 0);
	M.SetCount(Nb);
	for (int ib = 0; ib < Nb; ++ib) 
		M[ib].setConstant(6, 6, 0);
	
	Initialize_Forces(ex);
	Initialize_Forces(sc);
	Initialize_Forces(fk);
	Initialize_Forces(rao);
	
	Hydro::Initialize_MD(md, Nb, int(mdhead.size()), Nf);
		
	Hydro::Initialize_QTF(qtfsum, Nb, int(qh.size()), int(qw.size()));
	Hydro::Initialize_QTF(qtfdif, Nb, int(qh.size()), int(qw.size()));
			
	UArray<const UVector<double>*> Vos;
	UArray<const MatrixXd*> cgs, cbs, Ainfs, A0s;
	UArray<const UArray<UArray<VectorXd>>*> As, Bs;
	UArray<const UArray<MatrixXd>*> Cs, Ms;
	UArray<const Forces*> exs, scs, fks, raos;
	UArray<const UArray<UArray<UArray<VectorXd>>>*> mds;
	UArray<const UArray<UArray<UArray<MatrixXcd>>>*> qtfsums, qtfdifs;
	
	for (int i = 0; i < ids.size(); ++i) {
        if (hydros[i].hd().Vo.size() > i)
        	Vos.Add(&(hydros[i].hd().Vo));
        if (hydros[i].hd().cg.size() > 0)
        	cgs.Add(&(hydros[i].hd().cg));
        if (hydros[i].hd().cb.size() > 0)
        	cbs.Add(&(hydros[i].hd().cb));
        if (hydros[i].hd().Ainf.size() > 0)
        	Ainfs.Add(&(hydros[i].hd().Ainf));
        if (hydros[i].hd().A0.size() > 0)
        	A0s.Add(&(hydros[i].hd().A0));
        As.Add(&(hydros[i].hd().A));
        Bs.Add(&(hydros[i].hd().B));
        Cs.Add(&(hydros[i].hd().C));
        Ms.Add(&(hydros[i].hd().M));
        exs.Add(&(hydros[i].hd().ex));
        scs.Add(&(hydros[i].hd().sc));
        fks.Add(&(hydros[i].hd().fk));
        raos.Add(&(hydros[i].hd().rao));
        mds.Add(&(hydros[i].hd().md));
        qtfsums.Add(&(hydros[i].hd().qtfsum));
        qtfdifs.Add(&(hydros[i].hd().qtfdif));
    }
    
	AvgB(Vo, Vos);    
    AvgB(cg, cgs);    
    AvgB(cb, cbs); 
	AvgB(Ainf, Ainfs); 
	AvgB(A0, A0s); 
	
	AvgB(A, As); 
	AvgB(B, Bs);
	 
	AvgB(C, Cs);
	AvgB(M, Ms);
	
    AvgB(ex, exs);
    AvgB(sc, scs);
    AvgB(fk, fks);
    AvgB(rao, raos);
    
    AvgB(md, mds);
    
	AvgB(qtfsum, qtfsums);
    AvgB(qtfdif, qtfdifs);
    
    /*

  
    qtfsum = clone(hyd.qtfsum);
    qtfdif = clone(hyd.qtfdif);

	*/
	
	/*
	Ainf_w
	
	Dlin = clone(Dlin);
	Cmoor
   
    Kirf = clone(hyd.Kirf);
    Tirf = clone(hyd.Tirf);
    */
  
    
    
    
}

void Hydro::Symmetrize_Forces(bool xAxis) {
	if (!IsLoadedFex() && !IsLoadedFsc() && !IsLoadedFfk() && !IsLoadedRAO())
		return;
	
	UVector<double> newHead;
	for (int ih = 0; ih < Nh; ++ih) {
		FindAddRatio(newHead, FixHeading_180(head[ih]), 0.001);
		FindAddRatio(newHead, FixHeading_180(MirrorHead(head[ih], xAxis)), 0.001);
	}
	Sort(newHead);
	int newNh = newHead.size();
	
	Forces newex, newsc, newfk;
	RAO newrao;
	
	if (IsLoadedFex()) {
		Symmetrize_ForcesEach(ex, newex, newHead, newNh, xAxis);
		ex = pick(newex);
	}
	if (IsLoadedFsc()) {
		Symmetrize_ForcesEach(sc, newsc, newHead, newNh, xAxis);
		sc = pick(newsc);
	}
	if (IsLoadedFfk()) {
		Symmetrize_ForcesEach(fk, newfk, newHead, newNh, xAxis);
		fk = pick(newfk);
	}
	if (IsLoadedRAO()) {
		Symmetrize_ForcesEach(rao, newrao, newHead, newNh, xAxis);
		rao = pick(newrao);
	}
	head = pick(newHead);		// New headings are set between -180 and 180
	Nh = newNh;
}

void Hydro::RemoveThresDOF_A(double thres) {
	if (!IsLoadedA())
		return;
	for (int idf = 0; idf < 6*Nb; ++idf) {
		for (int jdf = 0; jdf < 6*Nb; ++jdf) {
			double mx = -DBL_MAX, mn = DBL_MAX;
			for (int ifr = 0; ifr < Nf; ifr++) {
				double val = A_ndim(ifr, idf, jdf);
				mx = max(mx, val);
				mn = min(mn, val);
			}
			double delta = mx - mn;
			if (IsNum(mx) && IsNum(mn)) {
				double res = 0;
				for (int ifr = 1; ifr < Nf; ifr++) 
					res += abs(A_ndim(ifr, idf, jdf) - A_ndim(ifr-1, idf, jdf));
				res /= delta*(Nf - 1);
				if (res > thres) {
					for (int ifr = 0; ifr < Nf; ifr++) 
						A[idf][jdf][ifr] = Null;
					Ainf(idf, jdf) = Null;		
				}
			}
		}
	}
}

void Hydro::RemoveThresDOF_B(double thres) {
	if (!IsLoadedB())
		return;
	for (int idf = 0; idf < 6*Nb; ++idf) {
		for (int jdf = 0; jdf < 6*Nb; ++jdf) {
			double mx = -DBL_MAX, mn = DBL_MAX;
			for (int ifr = 0; ifr < Nf; ifr++) {
				double val = B_ndim(ifr, idf, jdf);
				mx = max(mx, val);
				mn = min(mn, val);
			}
			double delta = mx - mn;
			if (IsNum(mx) && IsNum(mn)) {
				double res = 0;
				for (int ifr = 1; ifr < Nf; ifr++) 
					res += abs(B_ndim(ifr, idf, jdf) - B_ndim(ifr-1, idf, jdf));
				res /= delta*(Nf - 1);
				if (res > thres) {
					for (int ifr = 0; ifr < Nf; ifr++) 
						B[idf][jdf][ifr] = Null;
				}
			}
		}
	}
}

void Hydro::RemoveThresDOF_Force(Forces &f, double thres) {
	if (!IsLoadedForce(f))
		return;
	for (int h = 0; h < Nh; ++h) {
		for (int i = 0; i < 6*Nb; ++i) {
			double mx = -DBL_MAX, mn = DBL_MAX;
			for (int ifr = 0; ifr < Nf; ifr++) {
				double val = abs(F_ndim(f, h, ifr, i));
				mx = max(mx, val);
				mn = min(mn, val);
			}
			if (mx != -DBL_MAX && mn != DBL_MAX) {
				double delta = mx - mn;
				double res = 0;
				for (int ifr = 1; ifr < Nf; ifr++) 
					res += abs(F_ndim(f, h, ifr, i) - F_ndim(f, h, ifr-1, i));
				res /= delta*(Nf - 1);
				if (res > thres) {
					for (int ifr = 0; ifr < Nf; ifr++) 
						f.force[h](ifr, i) = Null;
				}
			}
		}
	}
}

void Hydro::Compare_rho(Hydro &a) {
	if (a.rho != rho)
		throw Exc(Format(t_("%s is not the same %f<>%f"), t_("Density rho"), a.rho, rho));
}

void Hydro::Compare_g(Hydro &a) {
	if (a.g != g)
		throw Exc(Format(t_("%s is not the same %f<>%f"), t_("Gravity g"), a.g, g));
}

void Hydro::Compare_h(Hydro &a) {
	if (a.h != h)
		throw Exc(Format(t_("%s is not the same %f<>%f"), t_("Water depth h"), a.h, h));
}

void Hydro::Compare_Nb(Hydro &a) {
	if (a.Nb != Nb)
		throw Exc(Format(t_("%s is not the same %d<>%d"), t_("Number of bodies"), a.Nb, Nb));
}

void Hydro::Compare_w(Hydro &a) {
	if (a.Nf != Nf)	
		throw Exc(Format(t_("%s is not the same %f<>%f"), t_("Number of frequencies"), a.Nf, Nf));
	for (int i = 0; i < a.Nf; ++i) {
		if (!EqualRatio(a.w[i], w[i], 0.0001))
			throw Exc(Format(t_("%s is not the same %f<>%f"), 
							Format(t_("#%d %s"), i+1, t_("frequency")), a.w[i], w[i]));
	}
}

void Hydro::Compare_head(Hydro &a) {
	if (a.Nh != Nh)	
		throw Exc(Format(t_("%s is not the same %f<>%f"), t_("Number of headings"), a.Nh, Nh));
	for (int i = 0; i < a.Nh; ++i) {
		if (a.head[i] != head[i])
			throw Exc(Format(t_("%s is not the same %f<>%f"), 
							Format(t_("#%d %s"), i+1, t_("frequency")), a.w[i], w[i]));
	}
}

void Hydro::Compare_A(Hydro &a) {
	for (int ifr = 0; ifr < a.Nf; ifr++) {
		for (int idf = 0; idf < 6*a.Nb; ++idf) {
			for (int jdf = 0; jdf < 6*a.Nb; ++jdf) {
				double Aa = a.A[idf][jdf][ifr];
				double Ab = A[idf][jdf][ifr];
				if (IsNum(Aa) && IsNum(Ab) && Aa != Ab)
					throw Exc(Format(t_("%s is not the same %f<>%f"), 
							Format(t_("%s[%d](%d, %d)"), t_("A"), ifr+1, idf+1, jdf+1), 
							Aa, Ab));
			}
		}
	}
}

void Hydro::Compare_B(Hydro &a) {
	for (int ifr = 0; ifr < a.Nf; ifr++) {
		for (int idf = 0; idf < 6*a.Nb; ++idf) {
			for (int jdf = 0; jdf < 6*a.Nb; ++jdf) {
				double Ba = a.B[idf][jdf][ifr];
				double Bb = B[idf][jdf][ifr];
				if (IsNum(Ba) && IsNum(Bb) && Ba != Bb)
					throw Exc(Format(t_("%s is not the same %f<>%f"), 
							Format(t_("%s[%d](%d, %d)"), t_("B"), ifr+1, idf+1, jdf+1), 
							Ba, Bb));
			}
		}
	}
}

void Hydro::Compare_C(Hydro &a) {
	for (int ib = 0; ib < a.Nb; ib++) {
		for (int idf = 0; idf < 6; ++idf) {
			for (int jdf = 0; jdf < 6; ++jdf) {
				double Ca = a.C[ib](idf, jdf);
				double Cb = C[ib](idf, jdf);
				if (IsNum(Ca) && IsNum(Cb) && !EqualRatio(Ca, Cb, 0.0001))
					throw Exc(Format(t_("%s is not the same %f<>%f"), 
							Format(t_("%s[%d](%d, %d)"), t_("C"), ib+1, idf+1, jdf+1), 
							Ca, Cb));
			}
		}
	}
}

void Hydro::Compare_cg(Hydro &a) {
	for (int i = 0; i < 3; i++) {
		for (int ib = 0; ib < a.Nb; ib++) {
			if (a.cg(i, ib) != cg(i, ib))
				throw Exc(Format(t_("%s is not the same %f<>%f"), 
						Format(t_("%s(%d, %d)"), t_("cg"), i+1, ib+1), 
							a.cg(i, ib), cg(i, ib)));
		}
	}
}

bool Hydro::SaveAs(String file, Function <bool(String, int)> Status, BEM_FMT type, int qtfHeading) {
	int realNh = Nh;
	int realNf = Nf;
	
	if (type == UNKNOWN) {
		String ext = ToLower(GetFileExt(file));
		
		if (ext == ".1" || ext == ".2" || ext == ".3" || ext == ".3sc" || ext == ".3fk" || 
			ext == ".hst" || ext == ".4" || ext == ".12s" || ext == ".12d") 
			type = Hydro::WAMIT_1_3;
		else if (ext == ".dat")
			type = Hydro::FAST_WAMIT;	
		else if (ext == ".bem")
			type = Hydro::BEMROSETTA;
		else
			throw Exc(Format(t_("Conversion to file type '%s' not supported"), file));
	}
	bool ret = false;
	if (type == WAMIT) {
		Wamit data(*bem, this);
		ret = data.Save_out(file);			
	} else if (type == WAMIT_1_3) {
		Wamit data(*bem, this);
		ret = data.Save(file, Status, true, qtfHeading);	
	} else if (type == FAST_WAMIT) {
		Fast data(*bem, this);
		ret = data.Save(file, Status, qtfHeading);		
	} else if (type == BEMROSETTA) {
		HydroClass data(*bem, this);
		ret = data.Save(file);		
	} else if (type == AQWA) {
		Aqwa data(*bem, this);
		ret = data.Save(file, Status);		
	}
	code = type;
	Nh = realNh;
	Nf = realNf;
	
	return ret;
}

void Hydro::Join(const UVector<Hydro *> &hydrosp) {
	name = t_("Joined files");
	for (int ihy = 0; ihy < hydrosp.size(); ++ihy) {
		const Hydro &hydro = *hydrosp[ihy];
		if (hydro.name.Find("Nemoh_Part") >= 0) {
			name = GetFileTitle(GetFileFolder(GetFileFolder(hydro.file)));
			break;
		}
	}
	
	dimen = false;
	g = bem->g;
	rho = bem->rho;
	len = 1;

	h = Null;
	for (int ihy = 0; ihy < hydrosp.size(); ++ihy) {
		const Hydro &hydro = *hydrosp[ihy];
		if (IsNum(hydro.h))
			h = hydro.h;
		else if (h != hydro.h)
			throw Exc(Format(t_("Water depth does not match between '%s'(%d) and '%s'(%d)"), 
					hydrosp[0]->name, hydrosp[0]->h, hydro.name, hydro.h));			
	}	
	if (!IsNum(h))
		throw Exc(t_("No water depth found in models"));
			
	Nb = Null;
	for (int ihy = 0; ihy < hydrosp.size(); ++ihy) {
		const Hydro &hydro = *hydrosp[ihy];
		if (!IsNum(Nb))
			Nb = hydro.Nb;
		else if (Nb != hydro.Nb)
			throw Exc(Format(t_("Number of bodies does not match between '%s'(%d) and '%s'(%d)"), 
					hydrosp[0]->name, hydrosp[0]->Nb, hydro.name, hydro.Nb));			
	}
	if (!IsNum(Nb))
		throw Exc(t_("No body found in models"));
		
	head.Clear();
	for (int ihy = 0; ihy < hydrosp.size(); ++ihy) {
		const Hydro &hydro = *hydrosp[ihy];
		if (hydro.IsLoadedFex() || hydro.IsLoadedFsc() || hydro.IsLoadedFfk()) {
			for (int ih = 0; ih < hydro.head.size(); ih++) {
				double head_v = hydro.head[ih];
				FindAddRatio(head, head_v, 0.001);
			}
		}
	}
	Nh = head.size();
	if (Nh == 0)
		throw Exc(t_("No head found in models"));
	
	w.Clear();
	for (int ihy = 0; ihy < hydrosp.size(); ++ihy) {
		const Hydro &hydro = *hydrosp[ihy];
		if (hydro.IsLoadedA() && hydro.IsLoadedB()) {
			for (int ifr = 0; ifr < hydro.w.size(); ifr++) {
				double w_v = hydro.w[ifr];
				FindAddRatio(w, w_v, 0.001);
			}
		}
	}
	Sort(w);
	Nf = w.size();
	T.Clear();
	for (int i = 0; i < Nf; ++i)
		T << 2*M_PI/w[i];
	
	if (Nf == 0)
		throw Exc(t_("No frequency found in models"));
	
	names.SetCount(Nb);
	dof.SetCount(Nb);
	cg.setConstant(3, Nb, NaNDouble);
	c0.setConstant(3, Nb, NaNDouble);
	cb.setConstant(3, Nb, NaNDouble);
	Vo.SetCount(Nb, NaNDouble);
	
	Initialize_AB(A);
	Initialize_AB(B);
	
	C.SetCount(Nb);
	for (int ib = 0; ib < Nb; ++ib) 
		C[ib].setConstant(6, 6, NaNDouble);
	
	Initialize_Forces(ex);
	Initialize_Forces(sc);
	Initialize_Forces(fk);
		
	for (int ihy = 0; ihy < hydrosp.size(); ++ihy) {
		const Hydro &hydro = *hydrosp[ihy];
		
		// All this block should have to be the same. Now it is not tested
		
		code = hydro.code;
		
		for (int ib = 0; ib < Nb; ++ib) {
			if (!hydro.names[ib].IsEmpty())
				names[ib] = hydro.names[ib];
			if (IsNum(hydro.Vo[ib]))
				Vo[ib] = hydro.Vo[ib];
			for (int i = 0; i < 3; ++i) {
				if (IsNum(hydro.cg(i, ib)))
					cg(i, ib) = hydro.cg(i, ib);
				if (IsNum(hydro.cb(i, ib)))
					cb(i, ib) = hydro.cb(i, ib);
				if (IsNum(hydro.c0(i, ib)))
					c0(i, ib) = hydro.c0(i, ib);
			}
			dof[ib] = hydro.dof[ib];
		}
		
		if (/*IsLoadedC() && */ hydro.IsLoadedC()) {
			for (int ib = 0; ib < Nb; ++ib) {
				for (int idf = 0; idf < 6; ++idf) 
					for (int jdf = 0; jdf < 6; ++jdf) 
						C[ib](idf, jdf) = hydro.C_ndim(ib, idf, jdf);
			}
		}
		///////////////////////////////////////////////////////////////////
		
		if (/*IsLoadedA() && IsLoadedB() && */hydro.IsLoadedA() && hydro.IsLoadedB()) {
			for (int ifrhy = 0; ifrhy < hydro.Nf; ++ifrhy) {
				int ifr = FindClosest(w, hydro.w[ifrhy]);
				for (int idf = 0; idf < 6*Nb; ++idf) {
					for (int jdf = 0; jdf < 6*Nb; ++jdf) {	
						if (IsNum(hydro.A[idf][jdf][ifrhy]))
							A[idf][jdf][ifr] = hydro.A_ndim(ifrhy, idf, jdf);
						if (IsNum(hydro.B[idf][jdf][ifrhy]))
							B[idf][jdf][ifr] = hydro.B_ndim(ifrhy, idf, jdf);
					}
				}
			}
		}	
		Add_Forces(ex, hydro, hydro.ex);
		Add_Forces(sc, hydro, hydro.sc);
		Add_Forces(fk, hydro, hydro.fk);
		
		if (/*IsLoadedRAO() && */hydro.IsLoadedRAO()) {
			for (int ihhy = 0; ihhy < Nh; ++ihhy) {
				int ih = FindClosest(head, hydro.head[ihhy]);
				for (int ifrhy = 0; ifrhy < Nf; ++ifrhy) {
					int ifr = FindClosest(w, hydro.w[ifrhy]);
					for (int idf = 0; idf < 6*Nb; ++idf) {	
						if (IsNum(rao.force[ihhy](ifrhy, idf))) 
							rao.force[ih](ifr, idf) = hydro.R(ihhy, ifrhy, idf);
					}
				}
			}
		}
	}
	
	// Aw0 has to be recalculated
	/*
	// A0 is set from the lower frequency data set
	
	int ihminw = -1;
	double minw = DBL_MAX;
	for (int ihy = 0; ihy < hydrosp.size(); ++ihy) {
		const Hydro &hydro = *hydrosp[ihy];
		if (hydro.IsLoadedAw0()) {
			double minwhy = Min(hydro.w);
			if (minw > minwhy) {
				ihminw = ihy;
				minw = minwhy;
			}
		}
	}
	if (ihminw >= 0) {
		for (int i = 0; i < 6*Nb; ++i) 
			for (int j = 0; j < 6*Nb; ++j) 
				Aw0(i, j) = hydrosp[ihminw]->Aw0_ndim(i, j);	
	}*/
	
	//bem->calcAinf = true;
	//bem->calcAinf_w = true;
	
	// sts has to be recalculated	
}

void Hydro::Report() const {
	BEM::Print("\n" + Format(t_("%s file '%s'"), GetCodeStr(), file));
	BEM::Print("\n" + Format(t_("g [m/s2]: %s, h [m]: %s, rho [kg/m3]: %s, length scale [m]: %s"), 
								S_g(), S_h(), S_rho(), S_len()));
	String freqs;
	if (w.IsEmpty()) 
		freqs = t_("NONE");
	else if (w.size() > 1) {
		String strDeltaH;
		if (GetIrregularFreq() < 0) 
			strDeltaH = Format(t_("delta %s [rad/s]"), FDS(w[1] - w[0], 8, false));
		else {
			String strHead;
			for (int i = 0; i < w.size(); ++i) {
				if (i > 0)
					strHead << ", ";
				strHead << w[i];
			}
			strDeltaH = Format(t_("Non constant delta (%s)"), strHead); 
		}
	 	freqs = Format(t_("%s to %s %s"), FDS(w[0], 8, false), 
	 									  FDS(w[w.size()-1], 8, false), strDeltaH);	
	} else
		freqs = Format(t_("%s [rad/s]"), FDS(w[0], 8, false));
	
	String heads;
	if (head.IsEmpty())
		heads = t_("NONE");
	else if (head.size() > 1) {
		String strDeltaH;
		if (GetIrregularHead() < 0) 
			strDeltaH = Format(t_("delta %.1f [º]"), head[1] - head[0]);
		else {
			String strHead;
			for (int i = 0; i < head.size(); ++i) {
				if (i > 0)
					strHead << ", ";
				strHead << head[i];
			}
			strDeltaH = Format(t_("Non constant delta (%s)"), strHead); 
		}
	 	heads = Format(t_("%.1f to %.1f %s"), head[0], head[head.size()-1], strDeltaH);	
	} else
		heads = Format(t_("%.1f [º]"), head[0]);
	
	BEM::Print("\n" + Format(t_("#freqs: %d (%s)"), Nf, freqs)); 
	BEM::Print("\n" + Format(t_("#1st order headings: %d (%s)"), Nh, heads)); 
	BEM::Print("\n" + Format(t_("#bodies: %d"), Nb));
	for (int ib = 0; ib < Nb; ++ib) {
		String str = Format("\n%d.", ib+1);
		if (names.size() > ib)
			str += " '" + names[ib] + "'";
		if (dof.size() > ib)
			str += S(" ") + t_("dof") + ": " + FormatInt(dof[ib]);
		if (Vo.size() > ib && IsNum(Vo[ib]))
			str += S(" ") + t_("vol [m3]") + ": " + FDS(Vo[ib], 8, false);
		if (cg.size() > 3*ib && IsNum(cg(0, ib)))
			str += " " + Format("Cg(%.3f, %.3f, %.3f)[m]", cg(0, ib), cg(1, ib), cg(2, ib));
		if (cb.size() > 3*ib && IsNum(cb(0, ib)))
			str += " " + Format("Cb(%.3f, %.3f, %.3f)[m]", cb(0, ib), cb(1, ib), cb(2, ib));
		if (c0.size() > 3*ib && IsNum(c0(0, ib)))
			str += " " + Format("C0(%.3f, %.3f, %.3f)[m]", c0(0, ib), c0(1, ib), c0(2, ib));
		
		BEM::Print(str);
	}
}

void Hydro::GetBodyDOF() {
	dof.Clear();	 dof.SetCount(Nb, 0);
	for (int ib = 0; ib < Nb; ++ib)
		for (int idf = 0; idf < 6; ++idf)
			if (IsAvailableDOF(ib, idf))
				dof[ib]++;
}

bool Hydro::AfterLoad(Function <bool(String, int)> Status) {
	if (!IsLoadedA0())  
		GetA0();
	
	if ((!IsLoadedAinf() || !IsLoadedKirf()) && bem->calcAinf) {
		if (!IsNum(bem->maxTimeA) || bem->maxTimeA == 0) {
			lastError = t_("Incorrect time for A∞ calculation. Please review it in Options");
			return false;
		}
		if (!IsNum(bem->numValsA) || bem->numValsA < 10) {
			lastError = t_("Incorrect number of time values for A∞ calculation. Please review it in Options");
			return false;
		}
		if (!IsLoadedKirf()) {
			if (Status && !Status(t_("Obtaining the Impulse Response Function"), 40)) {
				lastError = t_("Cancelled by the user");
				return false;
			}
			GetK_IRF(min(bem->maxTimeA, GetK_IRF_MaxT()), bem->numValsA);
		}
		if (!IsLoadedAinf()) {
			if (Status && !Status(t_("Obtaining the infinite-frequency added mass (A∞)"), 70)) {
				lastError = t_("Cancelled by the user");
				return false;
			}
			GetAinf();
		}
	}
	if (bem->calcAinf_w) {
		if (!IsLoadedKirf())
			GetK_IRF(min(bem->maxTimeA, GetK_IRF_MaxT()), bem->numValsA);
		if (!IsLoadedAinf())
			GetAinf();
		if (Status && !Status(t_("Obtaining the frequency-dependent infinite-frequency added mass (A∞(ω))"), 90)) {
			lastError = t_("Cancelled by the user");
			return false;
		}
		GetAinf_w();
	}
//	if (Ainf_w.size() == 0)
//		InitAinf_w();
//	if (Ainf.size() == 0)
//		Ainf.setConstant(Nb*6, Nb*6, 0);
	
	/*try {
		CheckNaN();
	} catch (Exc e) {
		lastError = e;
		return false;
	}*/
	
	return true;
}

int Hydro::GetW0() {
	for (int i = 0; i < w.size(); ++i) {	
		if (w[i] < 0.0001)
			return i;
	}
	return Null;
}

void Hydro::Get3W0(int &id1, int &id2, int &id3) {
	UVector<double> ww = clone(w);
	
	Sort(ww);
	id1 = FindAdd(w, ww[0]); 	
	id2 = FindAdd(w, ww[1]); 
	id3 = FindAdd(w, ww[2]); 
}

void Hydro::GetA0() {
	if (!IsLoadedA())
		return;
	
	int iw0 = GetW0();
	if (IsNum(iw0)) {
		A0.setConstant(Nb*6, Nb*6, Null);
		for (int i = 0; i < Nb*6; ++i)
	        for (int j = 0; j < Nb*6; ++j) 
				A0(i, j) = A[i][j][iw0];
	} else if (w.size() < 3)
		return;
	else { 
		int iw1, iw2, iw3;
		Get3W0(iw1, iw2, iw3);
		double wiw1 = w[iw1];
		double wiw2 = w[iw2];
		double wiw3 = w[iw3];
		if (wiw1 > 1. && wiw1 > 3*(wiw2 - wiw1))	// Too high to guess A[0]
			return;
		A0.setConstant(Nb*6, Nb*6, Null);
		for (int i = 0; i < Nb*6; ++i)
	        for (int j = 0; j < Nb*6; ++j) {
	            if (!IsNum(A[i][j][iw1]) || !IsNum(A[i][j][iw2]) || !IsNum(A[i][j][iw3]))
	                A0(i, j) = Null;
	            else
					A0(i, j) = QuadraticInterpolate<double>(0, wiw1, wiw2, wiw3, A[i][j][iw1], A[i][j][iw2], A[i][j][iw3]);
	        }
	}
}

String Hydro::C_units_base(int i, int j) {
	if (i == 2 && j == 2)
		return "N/m";
	else if ((i == 2 && j == 3) || (i == 2 && j == 4))
		return "N/rad";
	else if (i >= 3 && j >= 3)
		return "Nm/rad";
	else
		return "";
}

String Hydro::C_units(int i, int j) {
	String ret = C_units_base(i, j);
	if (ret.IsEmpty())
		return C_units_base(j, i);
	return ret;
}

void Hydro::SetC(int ib, const MatrixXd &K) {		// K is supposed to be dimensionalized
	if (C.IsEmpty())
		C.SetCount(Nb);
	if (C[ib].size() == 0)
		C[ib].setConstant(6, 6, Null); 
	for (int idf = 0; idf < 6; ++idf) {
		for (int jdf = 0; jdf < 6; ++jdf) {
			double k = dimen ? g_rho_dim()/g_rho_ndim() : g_rho_ndim()*pow(len, GetK_C(idf, jdf));
	      	C[ib](idf, jdf) = K(idf, jdf)/k;
		}
	}
}

int Hydro::GetIrregularHead() const {
	if (Nh <= 2)
		return -1;
	double delta0 = head[1] - head[0];
	for (int i = 1; i < Nh - 1; ++i) {
		double delta = head[i+1] - head[i];
		if (!EqualRatio(delta, delta0, 0.001))
			return i;
	}
	return -1;
}

int Hydro::GetIrregularFreq() const {
	if (Nf <= 2)
		return -1;
	double delta0 = w[1] - w[0];
	for (int i = 1; i < Nf - 1; ++i) {
		double delta = w[i+1] - w[i];
		if (!EqualRatio(delta, delta0, delta0/10))
			return i;
	}
	return -1;
}

double Hydro::g_dim() 		const {return bem->g;}					// Dimensionalize only with system data
double Hydro::g_ndim()		const {return IsNum(g) ? g : bem->g;}	// Nondimensionalize with model data, if possible
double Hydro::rho_dim() 	const {return bem->rho;}		
double Hydro::rho_ndim()	const {return IsNum(rho) ? rho : bem->rho;}
double Hydro::g_rho_dim() 	const {return bem->rho*bem->g;}
double Hydro::g_rho_ndim()	const {return g_ndim()*rho_ndim();}

void Hydro::StateSpace::GetTFS(const UVector<double> &w) {
	Eigen::Index sz = A_ss.rows();
	TFS.SetCount(w.size());
	for (int ifr = 0; ifr < w.size(); ++ifr) {
		std::complex<double> wi = std::complex<double>(0, w[ifr]);
		
		MatrixXcd Iwi_A = MatrixXd::Identity(sz, sz)*wi - A_ss;
		
		if (FullPivLU<MatrixXcd>(Iwi_A).isInvertible())
			TFS[ifr] = (C_ss.transpose()*Iwi_A.inverse()*B_ss)(0);	// C_ss*inv(I*w*i-A_ss)*B_ss
		else
			TFS[ifr] = Null;
	}		
}

int Hydro::GetHeadId(double hd) const {
	hd = FixHeading_180(hd);
	for (int i = 0; i < head.size(); ++i) {
		if (EqualRatio(FixHeading_180(head[i]), hd, 0.01))
			return i;
	}
	return -1;
}

int Hydro::GetHeadIdMD(const std::complex<double> &hd) const {
	std::complex<double> hd_ = FixHeading_180(hd);
	for (int i = 0; i < mdhead.size(); ++i) {
		if (EqualRatio(FixHeading_180(mdhead[i]), hd_, 0.01))
			return i;
	}
	return -1;
}

VectorXd Hydro::B_dim(int idf, int jdf) const {
	if (dimen)
		return B[idf][jdf]*(rho_dim()/rho_ndim());
	else {
		VectorXd ret = B[idf][jdf]*(rho_dim()*pow(len, GetK_AB(idf, jdf)));
		VectorXd ww = Get_w();
		return ret.array()*ww.array();
	}
}

VectorXd Hydro::B_ndim(int idf, int jdf) const {
	if (B[idf][jdf].size() == 0)
		return VectorXd();
	if (!dimen)
		return B[idf][jdf]*(rho_ndim()/rho_dim());
	else {
		VectorXd ret = B[idf][jdf]/(rho_ndim()*pow(len, GetK_AB(idf, jdf)));
		VectorXd ww = Get_w();
		return ret.array()/ww.array();
	}
}


void Hydro::GetOldAB(const UArray<MatrixXd> &oldAB, UArray<UArray<VectorXd>> &AB) {
	AB.Clear();
	int Nf = oldAB.size();
	int Nb = 0;
	if (Nf > 0)
		Nb = int(oldAB[0].rows())/6;
	AB.SetCount(6*Nb);
	for (int i = 0; i < 6*Nb; ++i) {
		AB[i].SetCount(6*Nb);
		for (int j = 0; j < 6*Nb; ++j) {
			AB[i][j].resize(Nf);	
			for (int idf = 0; idf < Nf; ++idf) 
				AB[i][j][idf] = oldAB[idf](i, j);	
		}
	}
}

void Hydro::SetOldAB(UArray<MatrixXd> &oldAB, const UArray<UArray<VectorXd>> &AB) {
	oldAB.Clear();
	int Nb = AB.size()/6;
	int Nf = 0;
	if (Nb > 0)
		Nf = int(AB[0][0].size());
	oldAB.SetCount(Nf);
	for (int idf = 0; idf < Nf; ++idf) {
		oldAB[idf].resize(6*Nb, 6*Nb);
		for (int i = 0; i < 6*Nb; ++i) 
			for (int j = 0; j < 6*Nb; ++j) 
				oldAB[idf](i, j) = AB[i][j][idf];
	}
}

MatrixXd Hydro::A_mat(bool ndim, int ifr, int ib1, int ib2) const {
	MatrixXd ret;
	if (!IsLoadedA())
		return ret;
	ret.resize(6, 6);
	for (int idf = 0; idf < 6; ++idf) 	
		for (int jdf = 0; jdf < 6; ++jdf) 
			ret(idf, jdf) = A_(ndim, ifr, idf + 6*ib1, jdf + 6*ib2);
	return ret;
}

MatrixXd Hydro::Ainf_mat(bool ndim, int ib1, int ib2) const {
	MatrixXd ret;
	if (!IsLoadedAinf())
		return ret;
	ret.resize(6, 6);
	for (int idf = 0; idf < 6; ++idf) 	
		for (int jdf = 0; jdf < 6; ++jdf) 
			ret(idf, jdf) = Ainf_(ndim, idf + 6*ib1, jdf + 6*ib2);
	return ret;
}

MatrixXd Hydro::B_mat(bool ndim, int ifr, int ib1, int ib2) const {
	MatrixXd ret;
	if (!IsLoadedA())
		return ret;
	ret.resize(6, 6);
	for (int idf = 0; idf < 6; ++idf) 	
		for (int jdf = 0; jdf < 6; ++jdf) 
			ret(idf, jdf) = B_(ndim, ifr, idf + 6*ib1, jdf + 6*ib2);
	return ret;
}

MatrixXd Hydro::C_(bool ndim, int ib) const {
	MatrixXd ret;
	if (C.IsEmpty())
		return ret;
	ret.resize(6, 6);
	for (int idf = 0; idf < 6; ++idf) 	
		for (int jdf = 0; jdf < 6; ++jdf) 
			ret(idf, jdf) = C_(ndim, ib, idf, jdf);
	return ret;
}

MatrixXd Hydro::Dlin_dim(int ib) const {
	MatrixXd ret;
	if (!IsLoadedDlin())
		return ret;
	ret.resize(6, 6);
	for (int idf = 0; idf < 6; ++idf) 	
		for (int jdf = 0; jdf < 6; ++jdf) 
			ret(idf, jdf) = Dlin(ib*6 + idf, ib*6 + jdf);
	return ret;
}

void Hydro::C_dim() {
	if (C.IsEmpty())
		return;
	for (int ib = 0; ib < Nb; ++ib) 
		for (int idf = 0; idf < 6; ++idf) 	
			for (int jdf = 0; jdf < 6; ++jdf) 
				C[ib](idf, jdf) = C_dim(ib, idf, jdf);
}

void Hydro::F_dim(Forces &f) {
	if (f.force.IsEmpty())
		return;
	for (int ih = 0; ih < Nh; ++ih) 	
		for (int ifr = 0; ifr < Nf; ++ifr)
			for (int idf = 0; idf < 6*Nb; ++idf) 
				f.force[ih](ifr, idf) = F_dim(f, ih, ifr, idf);
}

VectorXcd Hydro::F_(bool ndim, const Forces &f, int _h, int ifr) const {
	VectorXcd ret;
	if (f.force.IsEmpty())
		return ret;
	ret.resize(6);
	for (int idf = 0; idf < 6; ++idf) 
		ret[idf] = F_(ndim, f, _h, ifr, idf);
	return ret;
}

VectorXcd Hydro::F_dof(bool ndim, const Forces &f, int _h, int idf) const {
	VectorXcd ret;
	if (f.force.IsEmpty())
		return ret;
	ret.resize(Nf);
	for (int ifr = 0; ifr < Nf; ++ifr) 
		ret[ifr] = F_(ndim, f, _h, ifr, idf);
	return ret;
}

void Hydro::CheckNaN() {
	if (!IsNum(A))
		throw Exc("Error loading A. NaN found");
	if (!IsNum(Ainf_w))
		throw Exc("Error loading Ainfw. NaN found");
	if (!IsNum(Ainf))
		throw Exc("Error loading Awinf. NaN found");
	if (!IsNum(A0))
		throw Exc("Error loading A_0. NaN found");
	if (!IsNum(B))
		throw Exc("Error loading B. NaN found");
	if (!IsNum(head))
		throw Exc("Error loading head. NaN found");
	if (!IsNum(M))
		throw Exc("Error loading M. NaN found");
	if (!IsNum(C))
		throw Exc("Error loading C. NaN found");
	if (!IsNum(cb))
		throw Exc("Error loading cb. NaN found");
	if (!IsNum(cg))
		throw Exc("Error loading cg. NaN found");
	if (!IsNum(c0))
		throw Exc("Error loading c0. NaN found");
	if (!IsNum(dof))
		throw Exc("Error loading dof. NaN found");
	if (!IsNum(Kirf))
		throw Exc("Error loading Kirf. NaN found");
	if (!IsNum(Tirf))
		throw Exc("Error loading Tirf. NaN found");
	if (!IsNum(ex))
		throw Exc("Error loading ex. NaN found");
	if (!IsNum(sc))
		throw Exc("Error loading sc. NaN found");
	if (!IsNum(fk))
		throw Exc("Error loading fk. NaN found");
	if (!IsNum(rao))
		throw Exc("Error loading rao. NaN found");	
}

double Hydro::Tdof(int ib, int idf) const {
	if (!IsLoadedAinf(idf+6*ib, idf+6*ib) || !IsLoadedM(ib, idf, idf) || !IsLoadedC(ib, idf, idf))
		return Null;
	double m = M[ib](idf, idf);
	double a = Ainf_dim(ib*6+idf, ib*6+idf);
	double c = C_dim(ib, idf, idf);
	if (c == 0)
		return Null;
	double d = (m + a)/c;
	if (d < 0)
		return Null;
	return 2*M_PI*sqrt(d); 
}

double Hydro::Theave(int ib) const {
	return Tdof(ib, 2);
}

double Hydro::Troll(int ib) const {
	return Tdof(ib, 3); 
}

double Hydro::Tpitch(int ib) const {
	return Tdof(ib, 4); 
}

double Hydro::GM(int ib, int idf) const {
	if (Vo.size() <= ib || Vo[ib] == 0)
		return Null;
	double den = rho*g*Vo[ib];
	if (den == 0)
		return Null;
	return C_dim(ib, idf, idf)/den;
}

double Hydro::GMroll(int ib) const {
	return GM(ib, 3);
}

double Hydro::GMpitch(int ib) const {
	return GM(ib, 4);
}

void Hydro::Jsonize(JsonIO &json) {
	int icode;
	UArray<MatrixXd> oldA, oldB, oldKirf;
	if (json.IsStoring()) {
		icode = code;
		SetOldAB(oldA, A);
		SetOldAB(oldB, B);
		SetOldAB(oldKirf, Kirf);
	}
	json
		("file", file)
		("name", name)
		("g", g)
		("h", h)
		("rho", rho)
		("len", len)
		("dimen", dimen)
		("Nb", Nb)
		("Nf", Nf)
		("Nh", Nh)
		("A", oldA)
		("Awinf", Ainf)
		("Aw0", A0)
		("B", oldB)
		("head", head)
		("names", names)
		("C", C)
		("cb", cb)
		("cg", cg)
		("c0", c0)
		("code", icode)
		("dof", dof)
//		("dofOrder", dofOrder)
		("Kirf", oldKirf)
		("Tirf", Tirf)
		("ex", ex)
		("sc", sc)
		("fk", fk)
		("rao", rao)
		("sts", sts)
		("T", T)
		("w", w)
		("dataFromW", dataFromW)
		("Vo", Vo)
		("stsProcessor", stsProcessor)
		("dimenSTS", dimenSTS)
		("description", description)
		("qtfsum", qtfsum)
		("qtfdif", qtfdif)
		("Dlin", Dlin)
	;
	if(json.IsLoading()) {
		code = static_cast<Hydro::BEM_FMT>(icode);
		GetOldAB(oldA, A);
		GetOldAB(oldB, B);
		GetOldAB(oldKirf, Kirf);
	}
}
	
BEM::BEM() {
	bemFilesAst = clone(bstFilesExt);	// clone(bemFilesExt);
	bemFilesAst.Replace(".", "*.");
	experimental = ToLower(GetExeTitle()).Find("experimental") >= 0;
}

void BEM::LoadBEM(String file, Function <bool(String, int)> Status, bool checkDuplicated) {
	Status(t_("Loading files"), 10);
	if (checkDuplicated) {
		for (int i = 0; i < hydros.size(); ++i) {
			if (hydros[i].hd().file == file) 
				throw Exc(Format(t_("Model '%s' is already loaded"), file));
		}
	}
	SystemSignature sys;
	sys.Load();
		
	String ext = ToLower(GetFileExt(file));
	if (ext == ".cal" || ext == ".tec") {
		Nemoh &data = hydros.Create<Nemoh>(*this);
		if (!data.Load(file)) {
			String error = data.hd().GetLastError();
			hydros.SetCount(hydros.size()-1);
			throw Exc(Format(t_("Problem loading '%s'\n%s"), file, error));	
		}
	} else if (ext == ".inf") {
		Nemoh &data = hydros.Create<Nemoh>(*this);
		if (!data.Load(file)) {
			String error = data.hd().GetLastError();
			hydros.SetCount(hydros.size()-1);
			throw Exc(Format(t_("Problem loading '%s'\n%s"), file, error));	
		}
	} else if (ext == ".out") {
		Wamit &data = hydros.Create<Wamit>(*this);
		if (!data.Load(file, Status)) {
			String error = data.hd().GetLastError();
			hydros.SetCount(hydros.size()-1);
			throw Exc(Format(t_("Problem loading '%s'\n%s"), file, error));
		}
	} else if (ext == ".in") {
		HAMS &data = hydros.Create<HAMS>(*this);
		if (!data.Load(file, Status)) {
			String error = data.hd().GetLastError();
			hydros.SetCount(hydros.size()-1);
			throw Exc(Format(t_("Problem loading '%s'\n%s"), file, error));
		}
	} else if (ext == ".dat" || ext == ".fst") {
		Fast &data = hydros.Create<Fast>(*this);
		if (!data.Load(file, Status)) {
			String error = data.hd().GetLastError();
			hydros.SetCount(hydros.size()-1);
			throw Exc(Format(t_("Problem loading '%s'\n%s"), file, error));		
		}
	} else if (ext == ".1" || ext == ".2" || ext == ".3" || ext == ".3sc" || ext == ".3fk" || ext == ".7" || ext == ".8" || ext == ".9" ||
			   ext == ".hst" || ext == ".4" || ext == ".12s" || ext == ".12d" || ext == ".frc" || ext == ".pot") {
		Wamit &data = hydros.Create<Wamit>(*this);
		if (!data.Load(file, Status)) {
			String error = data.hd().GetLastError();
			hydros.SetCount(hydros.size()-1);
			throw Exc(Format(t_("Problem loading '%s'\n%s"), file, error));		
		}
	} else if (ext == ".ah1" || ext == ".lis" || ext == ".qtf") {
		Aqwa &data = hydros.Create<Aqwa>(*this);
		if (!data.Load(file)) {
			String error = data.hd().GetLastError();
			hydros.SetCount(hydros.size()-1);
			throw Exc(Format(t_("Problem loading '%s'\n%s"), file, error));	
		}
	} else if (ext == ".hdb") {
		Diodore &data = hydros.Create<Diodore>(*this);
		if (!data.Load(file)) {
			String error = data.hd().GetLastError();
			hydros.SetCount(hydros.size()-1);
			throw Exc(Format(t_("Problem loading '%s'\n%s"), file, error));	
		}
	} else if (ext == ".yml") {
		OrcaWave &data = hydros.Create<OrcaWave>(*this);
		if (!data.Load(file)) {
			String error = data.hd().GetLastError();
			hydros.SetCount(hydros.size()-1);
			throw Exc(Format(t_("Problem loading '%s'\n%s"), file, error));	
		}
	} else if (ext == ".mat") {
		Foamm &data = hydros.Create<Foamm>(*this);
		if (!data.Load(file)) {
			String error = data.hd().GetLastError();
			hydros.SetCount(hydros.size()-1);
			throw Exc(Format(t_("Problem loading '%s'\n%s"), file, error));	
		}
	} else if (ext == ".bem") {
		HydroClass &data = hydros.Create<HydroClass>(*this);
		if (!data.Load(file)) {
			String error = data.hd().GetLastError();
			hydros.SetCount(hydros.size()-1);
			throw Exc(Format(t_("Problem loading '%s'\n%s"), file, error));	
		}
	} else 
		throw Exc(Format(t_("Unknown BEM file extension in '%s'"), file));
	
	Hydro &justLoaded = hydros.Top().hd();

	if (!justLoaded.AfterLoad(Status)) {
		String error = RemoveAccents(justLoaded.GetLastError());
		hydros.SetCount(hydros.size()-1);
		throw Exc(Format(t_("Problem processing '%s'\n%s"), file, error));	
	}
	
	/*if (discardNegDOF) {
		if (Status && !Status(t_("Discarding negligible DOF"), 90)) {
			hydros.SetCount(hydros.size()-1);	
			throw Exc(t_("Cancelled by user"));
		}
		justLoaded.RemoveThresDOF_A(thres);
		justLoaded.RemoveThresDOF_B(thres);
		justLoaded.RemoveThresDOF_Force(justLoaded.ex, thres);
		justLoaded.RemoveThresDOF_Force(justLoaded.sc, thres);
		justLoaded.RemoveThresDOF_Force(justLoaded.fk, thres);
		justLoaded.RemoveThresDOF_Force(justLoaded.rao, thres);
	}*/
	if (hydros.size() == 1)
		Nb = justLoaded.Nb;
	else {
		if (justLoaded.Nb > Nb) {
			//int justLoaded_Nb = justLoaded.Nb;	Error removed for now
			//hydros.SetCount(hydros.size()-1);
			//throw Exc(Format(t_("Model has more bodies (%d) than previously loaded (%d)"), justLoaded_Nb, Nb));
			Nb = justLoaded.Nb;
		}
	}
	UpdateHeadAll();
	UpdateHeadAllMD();
}

HydroClass &BEM::Join(UVector<int> &ids, Function <bool(String, int)> Status) {
	UVector<Hydro *>hydrosp;
	
	hydrosp.SetCount(ids.size());
	for (int i = 0; i < ids.size(); ++i) 
		hydrosp[i] = &hydros[ids[i]].hd(); 
	
	HydroClass &data = hydros.Create<HydroClass>(*this);
	data.hd().Join(hydrosp);
	if (!data.hd().AfterLoad(Status)) {
		String error = data.hd().GetLastError();
		throw Exc(Format(t_("Problem joining models: '%s'\n%s"), error));	
	}
	Sort(ids, StdLess<int>());
	for (int i = ids.size()-1; i >= 0; --i)
		hydros.Remove(ids[i]);
	return data;
}

void BEM::RemoveHydro(int id) {
	hydros.Remove(id);
	if (hydros.IsEmpty())
		Hydro::ResetIdCount();
}

HydroClass &BEM::Duplicate(int id) {
	HydroClass &data = hydros.Create<HydroClass>(*this);
	data.hd().Copy(hydros[id].hd());
	
	return data;
}

HydroClass &BEM::Average(UVector<int> &ids) {
	HydroClass &data = hydros.Create<HydroClass>(*this);
	data.hd().Average(hydros, ids);
	
	return data;
}

void BEM::SymmetrizeForces(int id, bool xAxis) {
	hydros[id].hd().Symmetrize_Forces(xAxis);
	
	UpdateHeadAll();
	UpdateHeadAllMD();
}

void BEM::UpdateHeadAll() {
	headAll.Clear();
	//orderHeadAll.Clear();
				
	for (int id = 0; id < hydros.size(); ++id) {
		for (int ih = 0; ih < hydros[id].hd().head.size(); ++ih) 
			FindAddDelta(headAll, FixHeading(hydros[id].hd().head[ih], headingType), 0.1);
	}
	Sort(headAll);
	//orderHeadAll = GetSortOrder(headAll);
}

void BEM::UpdateHeadAllMD() {
	headAllMD.Clear();
	//orderHeadAll.Clear();
				
	for (int id = 0; id < hydros.size(); ++id) {
		for (int ih = 0; ih < hydros[id].hd().mdhead.size(); ++ih) 
			FindAddDelta(headAllMD, FixHeading(hydros[id].hd().mdhead[ih], headingType), 0.1);
	}
	Sort(headAllMD, [&](auto& a, auto& b)->bool const { 
		if (a.real() < b.real())
			return true; 
		else if (a.real() > b.real())
			return false;
		else
			return a.imag() < b.imag();	
	});
	//orderHeadAll = GetSortOrder(headAll);
}

void BEM::A0(int id) {
	hydros[id].hd().GetA0();
}

void BEM::Kirf(int id, double maxT) {
	hydros[id].hd().GetK_IRF(maxT, numValsA);
}

void BEM::Ainf(int id) {
	hydros[id].hd().GetAinf();
} 

void BEM::Ainf_w(int id) {
	hydros[id].hd().GetAinf_w();
}

void BEM::RAO(int id) {
	hydros[id].hd().GetRAO();
}

void BEM::Symmetrize(int id) {
	hydros[id].hd().Symmetrize();
}

void BEM::OgilvieCompliance(int id, bool zremoval, bool thinremoval, bool decayingTail, bool haskind, UVector<int> &vidof, UVector<int> &vjdof) {
	hydros[id].hd().GetOgilvieCompliance(zremoval, thinremoval, decayingTail, haskind, vidof, vjdof);
}

void BEM::ResetForces(int id, Hydro::FORCE force, bool forceMD, Hydro::FORCE forceQtf) {
	hydros[id].hd().ResetForces(force, forceMD, forceQtf);
}

void BEM::MultiplyDOF(int id, double factor, const UVector<int> &idDOF, bool a, bool b, bool diag, bool f, bool md, bool qtf) {
	hydros[id].hd().MultiplyDOF(factor, idDOF, a, b, diag, f, md, qtf);
}

void BEM::SwapDOF(int id, int ib, int idof1, int idof2) {
	hydros[id].hd().SwapDOF(ib, idof1, idof2);
}

void BEM::FillFrequencyGapsABForces(int id, bool zero, int maxFreq) {
	hydros[id].hd().FillFrequencyGapsABForces(zero, maxFreq);
}

void BEM::FillFrequencyGapsQTF(int id, bool zero, int maxFreq) {
	hydros[id].hd().FillFrequencyGapsQTF(zero, maxFreq);
}

void BEM::FillFrequencyGapsABForcesZero(int id) {
	hydros[id].hd().FillFrequencyGapsABForcesZero();
}

void BEM::FillFrequencyGapsQTFZero(int id) {
	hydros[id].hd().FillFrequencyGapsQTFZero();
}

void BEM::CopyQTF_MD(int id) {
	hydros[id].hd().CopyQTF_MD();
}

void BEM::DeleteHeadingsFrequencies(int id, const UVector<int> &idFreq, const UVector<int> &idFreqQTF, const UVector<int> &idHead, const UVector<int> &idHeadMD, const UVector<int> &idHeadQTF) {
	hydros[id].hd().DeleteFrequencies(idFreq);
	hydros[id].hd().DeleteFrequenciesQTF(idFreqQTF);
	hydros[id].hd().DeleteHeadings(idHead);
	hydros[id].hd().DeleteHeadingsMD(idHeadMD);
	hydros[id].hd().DeleteHeadingsQTF(idHeadQTF);
}

void BEM::TranslationTo(int id, double xto, double yto, double zto) {
	hydros[id].hd().GetTranslationTo(xto, yto, zto);
}


int BEM::LoadMesh(String fileName, Function <bool(String, int pos)> Status, bool cleanPanels, bool checkDuplicated) {
	Status(Format(t_("Loading mesh '%s'"), fileName), 10);
	
	if (checkDuplicated) {
		for (int i = 0; i < surfs.size(); ++i) {
			if (surfs[i].fileName == fileName) {
				BEM::Print(S("\n") + t_("Model is already loaded"));
				throw Exc(t_("Model is already loaded"));
			}
		}
	}
	UArray<Mesh> meshes;
	String error = Mesh::Load(meshes, fileName, rho, g, cleanPanels);
	if (!error.IsEmpty()) {
		BEM::Print("\n" + Format(t_("Problem loading '%s'") + S("\n%s"), fileName, error));
		throw Exc(Format(t_("Problem loading '%s'") + S("\n%s"), fileName, error));
	}
	int num = meshes.size();
	for (int i = 0; i < num; ++i)
		surfs.Add(pick(meshes[i]));
	return num;
}

void BEM::SaveMesh(String fileName, const UVector<int> &ids, Mesh::MESH_FMT type, double g, Mesh::MESH_TYPE meshType, bool symX, bool symY) {
	if (type == Mesh::UNKNOWN) {
		String ext = ToLower(GetFileExt(fileName));
		
		if (ext == ".gdf")
			type = Mesh::WAMIT_GDF;
		else if (ext == ".dat")
			type = Mesh::NEMOH_DAT;
		else if (ext == ".")
			type = Mesh::NEMOH_PRE;
		else if (ext == ".pnl")
			type = Mesh::HAMS_PNL;
		else if (ext == ".stl")
			type = Mesh::STL_TXT;
		else if (ext == ".mesh")
			type = Mesh::BEM_MESH;
		else
			throw Exc(Format(t_("Conversion to file type '%s' not supported"), fileName));
	}
	UArray<Mesh*> meshes;
	for (int id : ids)
		meshes << &surfs[id];
	
	Mesh::SaveAs(meshes, fileName, type, meshType, rho, g, symX, symY);
}

void BEM::HealingMesh(int id, bool basic, Function <bool(String, int)> Status) {
	Status(Format(t_("Healing mesh '%s'"), surfs[id].fileName), 10);
	Print(S("\n\n") + Format(t_("Healing mesh '%s'"), surfs[id].fileName));
	
	String ret;
	try {
		ret = surfs[id].Heal(basic, rho, g, Status);
	} catch (Exc e) {
		surfs.SetCount(surfs.size()-1);
		Print("\n" + Format(t_("Problem healing '%s': %s") + S("\n%s"), e));
		throw std::move(e);
	}
	if (!ret.IsEmpty()) {
		ret.Replace("\n", "\n- ");
		Print(ret);
	} else
		Print(S(". ") + t_("The mesh is in good condition"));
}

void BEM::OrientSurface(int id, Function <bool(String, int)> Status) {
	Status(Format(t_("Orienting surface mesh '%s'"), surfs[id].fileName), 10);
	Print(S("\n\n") + Format(t_("Orienting surface mesh '%s'"), surfs[id].fileName));
	
	try {
		surfs[id].Orient();
	} catch (Exc e) {
		surfs.SetCount(surfs.size()-1);
		Print("\n" + Format(t_("Problem orienting surface '%s': %s") + S("\n%s"), e));
		throw std::move(e);
	}
}

void BEM::UnderwaterMesh(int id, Function <bool(String, int pos)> Status) {
	Status(Format(t_("Getting underwater mesh '%s'"), surfs[id].fileName), 10);
	
	Mesh &mesh = surfs.Add();
	Mesh &orig = surfs[id];
	mesh.fileName = orig.fileName;
	
	try {
		mesh.mesh.CutZ(orig.mesh, -1);
	} catch (Exc e) {
		surfs.SetCount(surfs.size()-1);
		Print("\n" + Format(t_("Problem loading '%s': %s") + S("\n%s"), e));
		throw std::move(e);
	}
}

void BEM::RemoveMesh(int id) {
	surfs.Remove(id);
	if (surfs.IsEmpty())
		Mesh::ResetIdCount();
}

void BEM::JoinMesh(int idDest, int idOrig) {
	const Mesh &orig = surfs[idOrig];
	Mesh &dest = surfs[idDest];
	dest.fileName << "/" << orig.fileName;
	
	try {
		dest.Append(orig.mesh, rho, g);
		RemoveMesh(idOrig);
	} catch (Exc e) {
		surfs.SetCount(surfs.size()-1);
		Print("\n" + Format(t_("Problem loading '%s': %s") + S("\n%s"), e));
		throw std::move(e);
	}
}

UVector<int> BEM::SplitMesh(int id, Function <bool(String, int pos)> Status) {
	Status(Format(t_("Splitting mesh '%s'"), surfs[id].fileName), 0);
	Mesh &orig = surfs[id];
	
	UVector<int> ret;
	try {
		UVector<UVector<int>> sets = orig.mesh.GetPanelSets(Status);
		if (sets.size() == 1)
			return ret;
		for (int i = 0; i < sets.size(); ++i) {		
			Mesh &surf = surfs.Add();
			ret << surfs.size()-1-1;		// One more as id is later removed
			for (int ii = 0; ii < sets[i].size(); ++ii) 
				surf.mesh.panels << clone(orig.mesh.panels[sets[i][ii]]);	
			
			surf.mesh.nodes = clone(orig.mesh.nodes);
			surf.SetCode(orig.GetCode());
			surf.AfterLoad(rho, g, false, true);
		}
		RemoveMesh(id);
	} catch (Exc e) {
		surfs.SetCount(surfs.size() - ret.size());
		Print("\n" + Format(t_("Problem loading '%s': %s") + S("\n%s"), e));
		throw std::move(e);
	}
	return ret;
}

void BEM::AddFlatPanel(double x, double y, double z, double size, double panWidth, double panHeight) {
	try {
		Mesh &surf = surfs.Add();

		surf.SetCode(Mesh::EDIT);
		surf.mesh.AddFlatRectangle(panWidth, panHeight, size); 
		surf.mesh.Translate(x, y, z);
	} catch (Exc e) {
		surfs.SetCount(surfs.size() - 1);
		Print("\n" + Format(t_("Problem adding flat panel: %s"), e));
		throw std::move(e);
	}	
}

void BEM::AddRevolution(double x, double y, double z, double size, UVector<Pointf> &vals) {
	try {
		Mesh &surf = surfs.Add();

		surf.SetCode(Mesh::EDIT);
		surf.mesh.AddRevolution(vals, size); 
		surf.mesh.Translate(x, y, z);
	} catch (Exc e) {
		surfs.SetCount(surfs.size() - 1);
		Print("\n" + Format(t_("Problem adding revolution surface: %s"), e));
		throw std::move(e);
	}	
}

void BEM::AddPolygonalPanel(double x, double y, double z, double size, UVector<Pointf> &vals) {
	try {
		Mesh &surf = surfs.Add();

		surf.SetCode(Mesh::EDIT);
		surf.mesh.AddPolygonalPanel(vals, size, true); 
		surf.mesh.Translate(x, y, z);
	} catch (Exc e) {
		surfs.SetCount(surfs.size() - 1);
		Print("\n" + Format(t_("Problem adding revolution surface: %s"), e));
		throw std::move(e);
	}	
}

void BEM::AddWaterSurface(int id, char c) {
	try {
		Mesh &surf = surfs.Add();

		surf.SetCode(Mesh::EDIT);
		surf.mesh.AddWaterSurface(surfs[id].mesh, surfs[id].under, c); 
		
		if (c == 'r')
			surf.name = t_("Water surface removed");
		else if (c == 'f')
			surf.name = t_("Water surface");
		else if (c == 'e')
			surf.name = t_("Water surface extracted");
		surf.name = surfs[id].name + " " + surf.name;
		surf.fileName =  "";
		
		if (true) {		// Clean panels
			Surface::RemoveDuplicatedPanels(surf.mesh.panels);
			Surface::RemoveDuplicatedPointsAndRenumber(surf.mesh.panels, surf.mesh.nodes);
			Surface::RemoveDuplicatedPanels(surf.mesh.panels);
		}
		
		surf.AfterLoad(rho, g, false, false);
		
		//surf.Report(rho);
	} catch (Exc e) {
		surfs.SetCount(surfs.size() - 1);
		Print("\n" + Format(t_("Problem adding revolution surface: %s"), e));
		throw std::move(e);
	}	
}
			
bool BEM::LoadSerializeJson() {
	bool ret;
	String folder = AppendFileNameX(GetAppDataFolder(), "BEMRosetta");
	if (!DirectoryCreateX(folder))
		ret = false;
	else {
		String fileName = AppendFileNameX(folder, "configdata.cf");
		if (!FileExists(fileName)) 
			ret = false;
		else {
			String jsonText = LoadFile(fileName);
			if (jsonText.IsEmpty())
				ret = false;
			else {
				if (!LoadFromJson(*this, jsonText))
					ret = false;
				else
					ret = true;
			}
		}
	}
	if (!ret || IsNull(g)) 
		g = 9.81;
	if (!ret || IsNull(depth)) 
		depth = 100;
	if (!ret || IsNull(rho)) 
		rho = 1000;
	if (!ret || IsNull(len)) 
		len = 1;
	//if (!ret || IsNull(discardNegDOF))
	//	discardNegDOF = false;
	//if (!ret || IsNull(thres)) 
	//	thres = 0.01;
	if (!ret || IsNull(calcAinf))
		calcAinf = true;
	if (!ret || IsNull(calcAinf_w))
		calcAinf_w = true;
	if (!ret || IsNull(maxTimeA))
		maxTimeA = 120;
	if (!ret || IsNull(numValsA))
		numValsA = 1000;
	if (!ret || IsNull(onlyDiagonal))
		onlyDiagonal = false;
	if (!ret || IsNull(volWarning))	
		volWarning = 1;
	if (!ret || IsNull(volError))
		volError = 10;
			
	return ret;
}

bool BEM::ClearTempFiles() {
	String folder = GetTempFilesFolder();
	DeleteFolderDeepWildcardsX(folder, "*.*");	Sleep(100);
	return DirectoryCreateX(folder);
}
	
bool BEM::StoreSerializeJson() {
	String folder = AppendFileNameX(GetAppDataFolder(), "BEMRosetta");
	if (!DirectoryCreateX(folder))
		return 0;
	String fileName = AppendFileNameX(folder, "configdata.cf");
	return StoreAsJsonFile(*this, fileName, true);
}


bool HydroClass::Load(String file) {
	BEM::Print("\n\n" + Format(t_("Loading '%s'"), file));
	
	if (!LoadFromJsonFile(hd(), file)) {
		BEM::PrintError("\n" + Format(t_("Error loading '%s'"), file));
		hd().lastError = "\n" + Format(t_("Error loading '%s'"), file);
		return false;
	}
	hd().file = file;
	return true;
}
	
bool HydroClass::Save(String file) {
	BEM::Print("\n\n" + Format(t_("Saving '%s'"), file));
	if (!StoreAsJsonFile(hd(), file, true)) {
		BEM::PrintError("\n" + Format(t_("Error saving '%s'"), file));
		hd().lastError = "\n" + Format(t_("Error saving '%s'"), file);
		return false;
	}
	return true;
}

int IsTabSpace(int c) {
	if (c == '\t' || c == ' ' || c == '!')
		return true;
	return false;
}

UVector<int> NumSets(int num, int numsets) {
	ASSERT(numsets > 0);
	UVector<int> ret;
	ret.SetCount(numsets);
	
	for (int i = 0; numsets > 0; ++i) {
		int delta = int(num/numsets);
		ret[i] = delta;
		num -= delta;
		numsets--;
	}
	return ret;
}

String FormatWam(double d) {
	if (!IsNum(d))
		return "0.0";
	return (d >= 0 ? " " : "-") + Format("%12E", abs(d));
}

void LineParserWamit::LoadWamitJoinedFields(String _line) {	
	line = _line;
	fields.Clear();
	UVector<String> prefields = Split(line, IsTabSpace, true);
	for (int id = 0; id < prefields.size(); ++id) {
		String s = prefields[id];
		String ns;
		for (int i = 0; i < s.GetCount(); ++i) {	
			int c = s[i];
			if (c == '-') {
				if (i == 0)
					ns.Cat(c);
				else if (s[i-1] == 'E')
					ns.Cat(c);
				else {
					fields << ns;
					ns.Clear();
					ns.Cat(c);
				}
			} else
				ns.Cat(c);
		}
		fields << ns;
	}
}
	
BEMBody::BEMBody() {
	dof.SetCount(6, false);	
	cg = Vector3d::Zero();
	c0 = Vector3d::Zero();
	mass.setConstant(6, 6, 0);
	Dlin.setConstant(6, 6, 0);
	Dquad.setConstant(6, 6, 0);
	C.setConstant(6, 6, 0);
	Cadd.setConstant(6, 6, 0);
	Cext.setConstant(6, 6, 0);
	Aadd.setConstant(6, 6, 0);
}
	
int BEMBody::GetNDOF() const {
	int ret = 0;
	for (auto &d : dof)
		ret += d;
	return ret;
}

void BEMCase::Load(String file, const BEM &bem) {
	if (ToLower(GetFileName(file)) == "nemoh.cal") {
		if (!static_cast<NemohCase&>(*this).Load(file)) 
			throw Exc(Format(t_("Problem loading '%s' file"), file));
	} else if (ToLower(GetFileExt(file)) == ".in") {
		if (!static_cast<HamsCase&>(*this).Load(file)) 
			throw Exc(Format(t_("Problem loading '%s' file"), file));
	} else if (ToLower(GetFileExt(file)) == ".dat") {
		if (!static_cast<AQWACase&>(*this).Load(file)) 
			throw Exc(Format(t_("Problem loading '%s' file"), file));
	} else
		throw Exc(t_("Unknown BEM input format"));
	
	if (IsNull(rho))
		rho = bem.rho;
	if (IsNull(g))
		g = bem.g;	
}

void BEMCase::SaveFolder(String folder, bool bin, int numCases, int numThreads, const BEM &bem, int solver) const {
	if (solver <= CAPYTAINE)
		static_cast<const NemohCase &>(*this).SaveFolder(folder, bin, numCases, numThreads, bem, solver);
	else if (solver == HAMS)
		static_cast<const HamsCase &>(*this).SaveFolder(folder, bin, numCases, numThreads, bem, solver);
	else
		static_cast<const AQWACase &>(*this).SaveFolder(folder, bin, numCases, numThreads, bem, solver);
}

void BEMCase::BeforeSave(String folderBase, int numCases, bool deleteFolder) const {
	if (numCases < 1)
		throw Exc(Format(t_("Number cases must be higher than 1 (%d)"), numCases));
	
	if (numCases > Nf)
		throw Exc(Format(t_("Number of cases %d must not be higher than number of frequencies %d"), numCases, Nf));
	
	if (deleteFolder) {		// If called from GUI, user has been warned
		if (!DeleteFileDeepWildcardsX(folderBase))
			throw Exc(Format(t_("Impossible to clean folder '%s'. Maybe it is in use"), folderBase));
		Sleep(100);
	}
	if (!DirectoryCreateX(folderBase))
		throw Exc(Format(t_("Problem creating '%s' folder"), folderBase));
}

UVector<String> BEMCase::Check(int solver) const {
	UVector<String> ret;
	
	bool isNemoh = solver != BEMCase::HAMS;
	bool oneBody = solver == BEMCase::HAMS;
	
	if (isNemoh) 
		ret.Append(static_cast<const NemohCase &>(*this).Check());
	else if (solver == BEMCase::HAMS)
		ret.Append(static_cast<const HamsCase &>(*this).Check());
	
	if (IsNull(h) || h < -1 || h > 100000)
		ret << Format(t_("Incorrect depth %s"), FormatDoubleEmpty(h));

	if (IsNull(Nf) || Nf < 1 || Nf > 1000)
		ret << Format(t_("Incorrect number of frequencies %s"), FormatIntEmpty(Nf));
	if (IsNull(minF) || minF < 0)
		ret << Format(t_("Incorrect min frequency %s"), FormatDoubleEmpty(minF));
	if (IsNull(maxF) || maxF < minF)
		ret << Format(t_("Minimum frequency %s has to be lower than maximum frequency %s"), FormatDoubleEmpty(minF), FormatDoubleEmpty(maxF));	
	
	if (IsNull(Nh) || Nh < 1 || Nh > 1000)
		ret << Format(t_("Incorrect number of headings %s"), FormatIntEmpty(Nh));
	if (IsNull(minH) || minH < -180)
		ret << Format(t_("Incorrect min heading %s"), FormatDoubleEmpty(minH));
	if (IsNull(maxH) || maxH > 360)
		ret << Format(t_("Incorrect max heading %s"), FormatDoubleEmpty(maxH));
	if (maxH < minH)
		ret << Format(t_("Minimum heading %s has to be lower than maximum heading %s"), FormatDoubleEmpty(minH), FormatDoubleEmpty(maxH));	

	if(bodies.size() < 1)
		ret << t_("The case has to include at least one body");
	if (oneBody && bodies.size() > 1)
		ret << t_("This solver just processes one body");
	
	return ret;
}

String FormatDoubleEmpty(double val) {
	if (IsNull(val))
		return t_("'empty'");
	else
		return FDS(val, 10, false);
}

String FormatIntEmpty(int val) {
	if (IsNull(val))
		return t_("'empty'");
	else
		return FormatInt(val);
}

bool IsNum(const Hydro::Forces &f) {
	return IsNum(f.force);
}

