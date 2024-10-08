
#include "Consts.h"



#include <stdlib.h>
#include <cstring>
#include <iostream>
#include "pugixml.hpp"

#include "cross.h"
#include "types.h"
#include "Region.h"
#include "Geometry.h"
#include "Global.h"
#include "vtkOutput.h"
#include "utils.h"
#include "spline.h"
#include <assert.h>

#define NoneFlag ((big_flag_t) 0)

const int d3q27_vec[] = { 0,0,0,1,0,0,-1,0,0,0,1,0,1,1,0,-1,1,0,0,-1,0,1,-1,0,-1,-1,0,0,0,1,1,0,1,-1,0,1,0,1,1,1,1,1,-1,1,1,0,-1,1,1,-1,1,-1,-1,1,0,0,-1,1,0,-1,-1,0,-1,0,1,-1,1,1,-1,-1,1,-1,0,-1,-1,1,-1,-1,-1,-1,-1 };

const double topo_eps = 1e-6;
const double topo_eps_area = 1e-6;

/// Main constructor
/**
        Constructs the Geometry object based on size
        \param r Global Lattice region
        \param units_ Units object associated with the this Geometry object
*/
Geometry::Geometry(const lbRegion & r, const lbRegion & tr, const UnitEnv &units_, const Model * model_):model(model_), region(r), totalregion(tr), units(units_)
{
	geom.resize(region.sizeL());
    Q = NULL;
    for (size_t i = 0; i < region.sizeL(); i++) {
		geom[i] = 0;
    }
    output("Creating geom size: %ld\n", region.sizeL());
}

inline void Geometry::ActivateCuts() {
	if (Q == NULL) {
		Q = new cut_t[region.sizeL()*26];
		for (size_t i = 0; i < region.sizeL()*26; i++) {
			Q[i] = NO_CUT;
		}
	}
}

#define E(x) if (x) { return -1; }

/// Return rounded unit-free value of an xml attribute
/**
        Get the value of an xml attribute, calculate the units
        and round the result to full integer
        \param attr XML attribute
        \param def Default value to return in case there is no such attribute
*/
int Geometry::val(pugi::xml_attribute attr, int def)
{
    if (!attr) {
	return def;
    } else {
	return myround(units.alt(attr.value(), def));
    }
}

/// Return rounded unit-free value of an xml attribute
/**
        Get the value of an xml attribute, calculate the units
        and round the result to full integer
        \param attr XML attribute
        \param def Default value to return in case there is no such attribute
*/
double Geometry::val(pugi::xml_attribute attr, double def)
{
    if (!attr) {
	return def;
    } else {
	return units.alt(attr.value(), def);
    }
}

/// Return rounded unit-free value of an xml attribute (with no default)
/**
        Get the value of an xml attribute, calculate the units
        and round the result to full integer. Print error when no default.
        \param attr XML attribute
*/
int Geometry::val(pugi::xml_attribute attr)
{
    if (!attr) {
	error("Attribute without value and default\n");
	return -1;
    }
    return myround(units.alt(attr.value()));
}
/// Return rounded unit-free value of an xml attribute, extract first char as prefix 
/**
        Get the value of an xml attribute, calculate the units
        and round the result to full integer. Print error when no default.
        \param attr XML attribute
*/
int Geometry::val_p(pugi::xml_attribute attr, char* prefix)
{
    if (!attr) {
    error("Attribute without value and default\n");
    return -1;
    }
    std::string tmp = attr.value();
    *prefix = tmp[0];
    if (*prefix == '>' || *prefix == '<'){
        tmp = tmp.substr(1);
    } else {
        *prefix = '+';
    }
    return myround(units.alt(tmp));
}


/// Return unit-free value of an xml attribute
double Geometry::val_d(pugi::xml_attribute attr)
{
    return units.alt(attr.value());
}

/// Set the foreground flag/NodeType
/**
        Set the foreground flag, which will be used to fill
        all the geometry primitives later on.
        \param name Name of the flag/NodeType to set
*/
int Geometry::setFlag(const pugi::char_t * name)
{
    Model::NodeTypeFlag it = model->nodetypeflags.by_name(name);
    if (! it) {
	ERROR("Unknown flag (in xml): %s\n", name);
	return -1;
    }
    fg = it.flag;
    fg_mask = it.group_flag;
    fg_mode = MODE_OVERWRITE;
    return 0;
}

/// Set the forground flag mask
/**
        Set the foreground flag mask, which will be used to
        mask what is overwriten when filling with foreground flag
        in all the geometry primitives later on.
        \param name Name of the flag mask/NodeType to set
*/
int Geometry::setMask(const pugi::char_t * name)
{
    Model::NodeTypeGroupFlag it = model->nodetypegroupflags.by_name(name);
    if (! it) {
	error("Unknown mask (in xml): %s\n", name);
	return -1;
    }
    fg_mask = it.flag;
    return 0;
}

/// Set the forground flag mode
/**
        Set the foreground flag mode, by which the forground flag will be set
        \param name Name of the flag mask/NodeType to set
*/
int Geometry::setMode(const pugi::char_t * mode)
{
	if (strcmp(mode, "overwrite") == 0) {
		fg_mode = MODE_OVERWRITE;
	} else if (strcmp(mode, "fill") == 0) {
		fg_mode = MODE_FILL;
	} else if (strcmp(mode, "change") == 0) {
		fg_mode = MODE_CHANGE;
	} else {
		error("Unknown mode (in xml): %s\n", mode);
		return -1;
	}
	return 0;
}

void Geometry::setZone(int zone_number)
{
    fg      = (fg      &(~ NODE_SETTINGZONE )) |  (zone_number << ZONE_SHIFT);
    fg_mask =  fg_mask |   NODE_SETTINGZONE;
}

#define MAX_INT 2e9;

/// Sets a region according to the XML attributes
/**
        Sets a region with respect to the parent region basing
        on XML attributes like "nx","dx" and "dx"
        \param node XML element
*/
lbRegion Geometry::getRegion(const pugi::xml_node & node)
{
    lbRegion ret;
    pugi::xml_attribute attr;
    if (!node) {
	ret.dx = ret.dy = ret.dz = 0;
	ret.nx = ret.ny = ret.nz = 1;
	return ret;
    }
    if (strcmp(node.name(),"Geometry") == 0) return totalregion;
    ret = getRegion(node.parent());

    char side; 
    
    attr = node.attribute("dx");
    if (attr) {
	int w = val_p(attr, &side);
    
    if (side == '<') {
        w = ret.nx + w;
    } else if (side == '+') {
    	if (w < 0)
	        w = ret.nx + w;
    }

	ret.dx += w;
	ret.nx -= w;
    }
    attr = node.attribute("dy");
    if (attr) {
	int w = val_p(attr, &side);

    if (side == '<') {
        w = ret.ny + w;
    } else if (side == '+') {
    	if (w < 0)
	        w = ret.ny + w;
    }

	ret.dy += w;
	ret.ny -= w;
    }
    attr = node.attribute("dz");
    if (attr) {
	int w = val_p(attr, &side);
    if (side == '<') {
        w = ret.nz + w;
    } else if (side == '+') {
    	if (w < 0)
	        w = ret.nz + w;
    }
	ret.dz += w;
	ret.nz -= w;
    }

    attr = node.attribute("fx");
    if (attr) {
	int w = val(attr);
	if (w < 0)
	    w = ret.nx + w + ret.dx;
	ret.nx = w - ret.dx + 1;
    }
    attr = node.attribute("fy");
    if (attr) {
	int w = val(attr);
	if (w < 0)
	    w = ret.ny + w + ret.dy;
	ret.ny = w - ret.dy + 1;
    }
    attr = node.attribute("fz");
    if (attr) {
	int w = val(attr);
	if (w < 0)
	    w = ret.nz + w + ret.dz;
	ret.nz = w - ret.dz + 1;
    }

    attr = node.attribute("nx");
    if (attr)
	ret.nx = val(attr);
    attr = node.attribute("ny");
    if (attr)
	ret.ny = val(attr);
    attr = node.attribute("nz");
    if (attr)
	ret.nz = val(attr);

    return ret;
}

/// Fill a node with foreground flag
inline big_flag_t Geometry::Dot(int x, int y, int z)
{
    if (region.isIn(x, y, z)) {
	int i = region.offset(x, y, z);
	if (geom[i] & fg_mask) {
		if (fg_mode == MODE_FILL) return NoneFlag;
	} else {
		if (fg_mode == MODE_CHANGE) return NoneFlag;
	}
	return geom[i] = (geom[i] & (~fg_mask)) | fg;
    } else 
    return NoneFlag;
}

/// Check if a point is inside a sphere
inline int inSphere(double x, double y, double z)
{
    x = 2 * x - 1;
    y = 2 * y - 1;
    z = 2 * z - 1;
    return (x * x + y * y + z * z) < 1;
}

/// Check if a point is inside of a wedge
inline int inWedge(double x, double y, double z, const char *type)
{
    if (strcmp(type, "") == 0)
	type = "UpperLeft";
/// Original Version
//    if (strcmp(type, "UpperLeft") == 0) {
//	// default
//    } else if (strcmp(type, "UpperRight") == 0) {
//	x = 1. - x;
//    } else if (strcmp(type, "LowerLeft") == 0) {
//	y = 1. - y;
//    } else if (strcmp(type, "LowerRight") == 0) {
//	x = 1. - x;
//	y = 1. - y;
//    }
//    return (x - y) < 1e-10;

//JM version to allow x-z plane wedge
    double delta = 0;
    if (strcmp(type, "UpperLeft") == 0) {
	// default
	delta = x-y;
    } else if (strcmp(type, "UpperRight") == 0) {
	x = 1. - x;
	delta = x-y;
    } else if (strcmp(type, "LowerLeft") == 0) {
	y = 1. - y;
	delta = x-y;
    } else if (strcmp(type, "LowerRight") == 0) {
	x = 1. - x;
	y = 1. - y;
	delta = x-y;
    } else if (strcmp(type, "UpperLeftXZ") == 0) {
	// default
	delta = x-z;
    } else if (strcmp(type, "UpperRightXZ") == 0) {
	x = 1. - x;
	delta = x-z;
    } else if (strcmp(type, "LowerLeftXZ") == 0) {
	z = 1. - z;
	delta = x-z;
    } else if (strcmp(type, "LowerRightXZ") == 0) {
	x = 1. - x;
	z = 1. - z;
	delta = x-z;
    }
    return delta < 1e-10;
}

/// Transform STL points according to attributes of an XML element
inline int Geometry::transformSTL(int ntri, STL_tri * tri, pugi::xml_node n)
{
	for (int d = 0; d < 3; d++) {
		std::string d_char;
		int id1 = 0;
		int id2 = 0;
		if (d == 0) {
			d_char = "X"; id1 = 1; id2 = 2;
		} else if (d == 1) {
			d_char = "Y"; id1 = 2; id2 = 0;
		} else if (d == 2) {
			d_char = "Z"; id1 = 0; id2 = 1;
		} else {
			ERROR("This cannot happen."); return -1;
		}
		std::string attr_name = d_char + "rot";
		pugi::xml_attribute attr = n.attribute(attr_name.c_str());
		if (attr) {
			double v = units.alt(attr.value());
			debug1("STL: rotating %s axis %lf (%lf deg)\n", d_char.c_str(), v, v / 4. / atan(1.) * 180);
			double y, z;
			for (int i = 0; i < ntri; i++) {
			    y = tri[i].p1[id1];
			    z = tri[i].p1[id2];
			    tri[i].p1[id1] = cos(v) * y - sin(v) * z;
			    tri[i].p1[id2] = sin(v) * y + cos(v) * z;
			    y = tri[i].p2[id1];
			    z = tri[i].p2[id2];
			    tri[i].p2[id1] = cos(v) * y - sin(v) * z;
			    tri[i].p2[id2] = sin(v) * y + cos(v) * z;
			    y = tri[i].p3[id1];
			    z = tri[i].p3[id2];
			    tri[i].p3[id1] = cos(v) * y - sin(v) * z;
			    tri[i].p3[id2] = sin(v) * y + cos(v) * z;
			}
		}
	}
    if (n.attribute("scale")) {
	double v = units.alt(n.attribute("scale").value());
	debug1("STL: scaling %lf\n", v);
	for (int i = 0; i < ntri; i++) {
	    tri[i].p1[0] = v * tri[i].p1[0];
	    tri[i].p2[0] = v * tri[i].p2[0];
	    tri[i].p3[0] = v * tri[i].p3[0];
	    tri[i].p1[1] = v * tri[i].p1[1];
	    tri[i].p2[1] = v * tri[i].p2[1];
	    tri[i].p3[1] = v * tri[i].p3[1];
	    tri[i].p1[2] = v * tri[i].p1[2];
	    tri[i].p2[2] = v * tri[i].p2[2];
	    tri[i].p3[2] = v * tri[i].p3[2];
	}
    }
    if (n.attribute("x")) {
	double v = units.alt(n.attribute("x").value());
	debug1("STL: move in x %lf\n", v);
	for (int i = 0; i < ntri; i++) {
	    tri[i].p1[0] += v;
	    tri[i].p2[0] += v;
	    tri[i].p3[0] += v;
	}
    }
    if (n.attribute("y")) {
	double v = units.alt(n.attribute("y").value());
	debug1("STL: move in y %lf\n", v);
	for (int i = 0; i < ntri; i++) {
	    tri[i].p1[1] += v;
	    tri[i].p2[1] += v;
	    tri[i].p3[1] += v;
	}
    }
    if (n.attribute("z")) {
	double v = units.alt(n.attribute("z").value());
	debug1("STL: move in z %lf\n", v);
	for (int i = 0; i < ntri; i++) {
	    tri[i].p1[2] += v;
	    tri[i].p2[2] += v;
	    tri[i].p3[2] += v;
	}
    }
    const double sm_diff[3] = {0.1403e-5, 0.1687e-5, 0.1987e-5};
    for (int i = 0; i < ntri; i++) {
	for (int j = 0; j < 3; j++) {
		tri[i].p1[j] = myround(tri[i].p1[j] * 1e5) * 1e-5;
		tri[i].p2[j] = myround(tri[i].p2[j] * 1e5) * 1e-5;
		tri[i].p3[j] = myround(tri[i].p3[j] * 1e5) * 1e-5;
		tri[i].p1[j] += sm_diff[j] - 0.5;
		tri[i].p2[j] += sm_diff[j] - 0.5;
		tri[i].p3[j] += sm_diff[j] - 0.5;
	}
    }
    return 0;
}

inline cut_t smaller(cut_t a,cut_t b) {
	if (a<b) return a;
	return b;
}

cut_t calcCut(STL_tri tri, double x, double y, double z, double dx, double dy, double dz) {
	double mat[16],b[4],r[4];
	mat[0] = -tri.p1[0] + x;
	mat[1] = -tri.p1[1] + y;
	mat[2] = -tri.p1[2] + z;
	mat[3] = 1;
	mat[4] = -tri.p2[0] + x;
	mat[5] = -tri.p2[1] + y;
	mat[6] = -tri.p2[2] + z;
	mat[7] = 1;
	mat[8] = -tri.p3[0] + x;
	mat[9] = -tri.p3[1] + y;
	mat[10] = -tri.p3[2] + z;
	mat[11] = 1;
	mat[12] = dx;
	mat[13] = dy;
	mat[14] = dz;
	mat[15] = 0;
	b[0] = 0;
	b[1] = 0;
	b[2] = 0;
	b[3] = 1;
	GaussSolve(mat,b,r,4);
	if (r[0] < 0) return NO_CUT;
	if (r[1] < 0) return NO_CUT;
	if (r[2] < 0) return NO_CUT;
	if (r[3] < 0) return NO_CUT;
	if (r[3] > 1) return NO_CUT;
	return r[3]*CUT_MAX;
}


/// Load STL file
inline int Geometry::loadSTL(lbRegion reg, pugi::xml_node n)
{
    char header[80];
    STL_tri *tri;
    int ntri;
    int ret;
    int insideOut=0;
    int axis = 1;
    if (!n.attribute("file")) {
		error("No 'file' attribute in 'STL' element in xml conf\n");
		return -1;
    }
    if (n.attribute("side")) {
        std::string side=n.attribute("side").value();
        if (side == "in") {
		    insideOut=0;
        } else if (side == "out") {
            insideOut=1;
        } else if (side == "surface") {
            insideOut=2;
        } else {
			error("'side' in 'STL' element have to be 'in', 'out' or 'surface'\n");
			return -1;
		}
    }
    if (n.attribute("ray_axis")) {
		if (insideOut > 1) {
			error("the 'ray_axis' setting in 'STL' makes sense only with 'in' or 'out' side\n");
			return -1;
		}
        std::string ray_axis=n.attribute("ray_axis").value();
        if (ray_axis == "x") {
			axis=0;
        } else if (ray_axis == "y") {
            axis=1;
        } else if (ray_axis == "z") {
            axis=2;
        } else {
			error("'ray_axis' in 'STL' element have to be 'x', 'y' or 'z'\n");
			return -1;
		}
    }
    debug1("------ STL -----\n");
    FILE *f = fopen(n.attribute("file").value(), "rb");
    if (f == NULL) {
		error("'STL' element: %s doesn't exists or cannot be opened\n", n.attribute("file").value());
		return -1;
    }
    ret = fread(header, 80, sizeof(char), f);
    ret = fread(&ntri, 1, sizeof(int), f);
    if (!strncmp(header, "solid", 5)){      // Checking if STL is binary. STL in ASCII begins with "solid"
        error("'STL' element %s is not in binary format!\n", n.attribute("file").value());
        debug1("'solid' found at the beginning of STL file");
        return -1;
    }
    tri = (STL_tri *) malloc(ntri * sizeof(STL_tri));
    char *lev = (char *) malloc(reg.sizeL() * sizeof(char));
    for (size_t i = 0; i < reg.sizeL(); i++)
	lev[i] = insideOut;
    debug1("%d x %ld = %ld\n", ntri, sizeof(STL_tri), ntri * sizeof(STL_tri));
    ret = fread(tri, ntri, sizeof(STL_tri), f);
    fclose(f);

    debug1("Number of triangles: %d\n", ntri);
    transformSTL(ntri, tri, n);
	size_t topo_hit[5];
	for (int i = 0; i < 5; i++) topo_hit[i] = 0;
	
    for (int i = 0; i < ntri; i++) {
	int min[3], max[3];
	for (int j=0; j<3;j++) {
		min[j] = ceil(tri[i].p1[j]);
		if (tri[i].p2[j] < min[j])
		    min[j] = ceil(tri[i].p2[j]);
		if (tri[i].p3[j] < min[j])
		    min[j] = ceil(tri[i].p3[j]);
		max[j] = floor(tri[i].p1[j]);
		if (tri[i].p2[j] > max[j])
		    max[j] = floor(tri[i].p2[j]);
		if (tri[i].p3[j] > max[j])
		    max[j] = floor(tri[i].p3[j]);
		min[j] -= 1;
		max[j] += 1;
	}
	if (insideOut == 2) {
            size_t regsize = region.sizeL();
            for (int x = min[0]; x <= max[0]; x++)
                for (int z = min[2]; z <= max[2]; z++)
                    for (int y = min[1]; y <= max[1]; y++) if (region.isIn(x, y, z)) {
                            size_t k = region.offset(x, y, z);
                            cut_t nq;
                            ActivateCuts();
                            for (int d = 0; d<26; d++) {
                                    nq = calcCut(tri[i],x,y,z,d3q27_vec[(d+1)*3],d3q27_vec[(d+1)*3+1],d3q27_vec[(d+1)*3+2]);
                                    if (nq < Q[regsize*d+k]) {
                                            Q[regsize*d+k] = nq;
									}								
									if (nq != NO_CUT){
										Dot(x, y, z);
									}
                            }
                    }
	} else {
		int ax1 = axis;
		int ax2 = (axis+1) % 3;
		int ax3 = (axis+2) % 3;
		int lx1 = 0;
		if (ax1 == 0) lx1 = reg.dx;
		if (ax1 == 1) lx1 = reg.dy;
		if (ax1 == 2) lx1 = reg.dz;
		double v[2], v1[2], v2[2], c0, c1, c2, c3;
		v1[0] = tri[i].p2[ax2] - tri[i].p1[ax2];
		v1[1] = tri[i].p2[ax3] - tri[i].p1[ax3];
		v2[0] = tri[i].p3[ax2] - tri[i].p1[ax2];
		v2[1] = tri[i].p3[ax3] - tri[i].p1[ax3];
		c0 = v1[0] * v2[1] - v1[1] * v2[0];
		if (fabs(c0) < topo_eps_area) {
			topo_hit[4]++;
		} else {
            for (int x2 = min[ax2]; x2 <= max[ax2]; x2++)
                for (int x3 = min[ax3]; x3 <= max[ax3]; x3++) {
                    v[0] = x2 - tri[i].p1[ax2];
                    v[1] = x3 - tri[i].p1[ax3];
                    c1 = v1[0] * v[1] - v1[1] * v[0];
                    c2 = v[0] * v2[1] - v[1] * v2[0];
                    c1 /= c0;
                    c2 /= c0;
					if (fabs(c1) < topo_eps) c1 = 0;
					if (fabs(c2) < topo_eps) c2 = 0;
                    c3 = 1. - c1 - c2;
					if (fabs(c3) < topo_eps) { c1 += c3/2; c2 += c3/2; c3 = 0; }
					int topo=0;
					const double dv[2] = { -0.5694552,  0.8220224}; // random direction for resolving bad edges
					double dc1 = (v1[0] * dv[1] - v1[1] * dv[0])/c0;
					double dc2 = (dv[0] * v2[1] - dv[1] * v2[0])/c0;
					double dc3 = - dc1 - dc2;
					if (c1 == 0) {
						topo_hit[1]++;
						if (dc1 > 0) topo++; else if (dc1 == 0) topo_hit[3]++; else topo_hit[2]++;
					} else if (c1 > 0) topo++;
					if (c2 == 0) {
						topo_hit[1]++;
						if (dc2 > 0) topo++; else if (dc2 == 0) topo_hit[3]++; else topo_hit[2]++;
					} else if (c2 > 0) topo++;
					if (c3 == 0) {
						topo_hit[1]++;
						if (dc3 > 0) topo++; else if (dc3 == 0) topo_hit[3]++; else topo_hit[2]++;
					} else if (c3 > 0) topo++;
					if (topo == 3) {
						topo_hit[0]++;
						double h = tri[i].p1[ax1] * c3 + tri[i].p2[ax1] * c2 + tri[i].p3[ax1] * c1;
						for (int x1 = lx1; x1 <= h; x1++) {
							int x,y,z;
							if (axis == 0) { x = x1; y = x2; z = x3; }
							if (axis == 1) { x = x3; y = x1; z = x2; }
							if (axis == 2) { x = x2; y = x3; z = x1; }
							if (reg.isIn(x, y, z)) lev[reg.offset(x, y, z)]++;
                        }
                    }
                }
			}
		}
    }
    if (insideOut != 2) {
        for (int x = reg.dx; x < reg.dx + reg.nx; x++)
            for (int y = reg.dy; y < reg.dy + reg.ny; y++)
                for (int z = reg.dz; z < reg.dz + reg.nz; z++) {
                    if (lev[reg.offset(x, y, z)] % 2 == 1) {
                        Dot(x, y, z);
                    }
                }
		output("STL: triangle hits: %ld\n", topo_hit[0]);
		if (topo_hit[4] > 0) notice("STL: - eps-area triangles omitted: %ld\n", topo_hit[4]);
		if (topo_hit[1] > 0) {	
			notice("STL: - edge hits: %ld\n", topo_hit[1]);
			notice("STL:   - resolved negatively: %ld\n", topo_hit[2]);
			notice("STL:   - resolved positively: %ld\n", topo_hit[1] - topo_hit[3] - topo_hit[2]);
			if (topo_hit[3] > 0) NOTICE("STL:   - could not be resolved: %ld (this can cause problems!)\n", topo_hit[3]);
		}
    }
    free(tri);
    free(lev);
    return 0;
}


/// Load a b-spline string
inline int Geometry::loadSweep(lbRegion reg, pugi::xml_node node)
{
	pugi::xml_attribute attr;

	int order=1;
	attr = node.attribute("order");
	if (attr) order = attr.as_int();
	double dl=1e-4;
	attr = node.attribute("step");
	if (attr) dl = attr.as_double();
	attr = node.attribute("steps");
	if (attr) dl = 1/units.alt(attr.value());
	double def_r=1;
	attr = node.attribute("r");
	if (attr) def_r = units.alt(attr.value());
	
	int n = 0;
	std::vector<double> nx,ny,nz,nr;
	for (pugi::xml_node par = node.first_child(); par; par = par.next_sibling()) {
		if (strcmp(par.name(),"Point") == 0) {
			nx.push_back(units.alt(par.attribute("x").value()));
			ny.push_back(units.alt(par.attribute("y").value()));
			nz.push_back(units.alt(par.attribute("z").value()));
			nr.push_back(val(par.attribute("r"),def_r));
			n++;
		} else {
			NOTICE("Unknown element %s in Sweep\n", par.name());
		}
	}
	if (n == 0) {
		NOTICE("No points in Sweep\n");
		return 0;
	}
	if (order > n-1) {
		NOTICE("For order %d b-spline you need at least %d points\n", order, order+1);
		order = n-1;
	}
    output("Making a Sweep over region (%ld) with %lf step\n",reg.size(), dl);

	double x0_=-999.0, y0_=-999.0, z0_=-999.0, r0_=-999.0;
    for (double l=0;l<1; l+= dl) {
		double x0 = bspline(l, nx, order);
		double y0 = bspline(l, ny, order);
		double z0 = bspline(l, nz, order);
		double r = bspline(l, nr, order);

		if (abs(x0-x0_) < 0.25 && 
			abs(y0-y0_) < 0.25 && 
			abs(z0-z0_) < 0.25 && 
			abs(r-r0_) < 0.25) {
				continue;		
		}
		x0_ = x0; y0_ = y0; z0_ = z0; r0_ = r;

		lbRegion tmpReg=reg.intersect(lbRegion(x0-r-1,y0-r-1,z0-r-1,2*r+2,2*r+2,2*r+2));
		for (int x = tmpReg.dx; x < tmpReg.dx + tmpReg.nx; x++)
		for (int y = tmpReg.dy; y < tmpReg.dy + tmpReg.ny; y++)
		for (int z = tmpReg.dz; z < tmpReg.dz + tmpReg.nz; z++) {
			if ((.5+x-x0)*(.5+x-x0)+(.5+y-y0)*(.5+y-y0)+(.5+z-z0)*(.5+z-z0) < r*r) {
				Dot(x, y, z);
			}
		}
	}

    return 0;	
}


/// Main geometry-generating function
int Geometry::Draw(pugi::xml_node & node)
{
    for (pugi::xml_node n = node.first_child(); n; n = n.next_sibling()) {
	lbRegion reg = getRegion(n);
	if (strcmp(n.name(), "Box") == 0) {
	    reg = region.intersect(reg);
	    debug1("Filling with flag %d (%d)\n", fg, fg_mask);
	    DEBUG1(reg.print();)
		for (int x = reg.dx; x < reg.dx + reg.nx; x++)
		for (int y = reg.dy; y < reg.dy + reg.ny; y++)
		    for (int z = reg.dz; z < reg.dz + reg.nz; z++) {
			Dot(x, y, z);
		    }
	} else if (strcmp(n.name(), "HalfSphere") == 0) {
//                      reg = region.intersect(reg);
	    debug1("Filling half sphere with flag %d (%d)\n", fg, fg_mask);
	    DEBUG1(reg.print();)
		for (int x = reg.dx; x < reg.dx + reg.nx; x++)
		for (int y = reg.dy; y < reg.dy + reg.ny; y++)
		    for (int z = reg.dz; z < reg.dz + reg.nz; z++) {
			if (inSphere((.5 + x - reg.dx) / reg.nx, 0.5 - (.5 + y - reg.dy) / reg.ny / 2., (.5 + z - reg.dz) / reg.nz) ) {
			    Dot(x, y, z);
			}
		    }
	} else if (strcmp(n.name(), "Sphere") == 0) {
//                      reg = region.intersect(reg);
	    debug1("Filling sphere with flag %d (%d)\n", fg, fg_mask);
	    DEBUG1(reg.print();)
		for (int x = reg.dx; x < reg.dx + reg.nx; x++)
		for (int y = reg.dy; y < reg.dy + reg.ny; y++)
		    for (int z = reg.dz; z < reg.dz + reg.nz; z++) {
			if (inSphere((.5 + x - reg.dx) / reg.nx, (.5 + y - reg.dy) / reg.ny, (.5 + z - reg.dz) / reg.nz)) {
			    Dot(x, y, z);
			}
		    }
    } else if (strcmp(n.name(), "OffgridSphere") == 0) {

        double x0 = units.alt(n.attribute("x").value());
        double y0 = units.alt(n.attribute("y").value());
        double z0 = units.alt(n.attribute("z").value());

        double Rx,Ry,Rz;
        if ( n.attribute("R").empty() ){
           Rx = units.alt(n.attribute("Rx").value());
           Ry = units.alt(n.attribute("Ry").value());
           Rz = units.alt(n.attribute("Rz").value());
        } else {
           double R = units.alt(n.attribute("R").value());
           Rx = R;
           Ry = R;
           Rz = R;
        }

        double Rx2 = Rx*Rx;
        double Ry2 = Ry*Ry;
        double Rz2 = Rz*Rz;
   
        reg.dx = x0 - Rx - 5;
        reg.dy = y0 - Ry - 5;
        reg.dz = z0 - Rz - 5;

        reg.nx = 2*Rx + 10;
        reg.ny = 2*Ry + 10;
        reg.nz = 2*Rz + 10;

	    debug1("Filling sphere with flag %d (%d)\n, with center at (%lf,%lf,%lf) and radius (%lf,%lf,%lf)", fg, fg_mask, x0, y0, z0, Rx,Ry,Rz);
	    DEBUG1(reg.print();)
		for (int x = reg.dx; x < reg.dx + reg.nx; x++)
		for (int y = reg.dy; y < reg.dy + reg.ny; y++)
		    for (int z = reg.dz; z < reg.dz + reg.nz; z++) {
               double xx =  (.5 + x - x0);  
               double yy =  (.5 + y - y0);  
               double zz =  (.5 + z - z0);  
			   if ( xx*xx/Rx2 + yy*yy/Ry2 + zz*zz/Rz2 < 1. ) {
			    Dot(x, y, z);
			   }
		}
	}  else if (strcmp(n.name(), "OffgridPipe") == 0) {

        double x0 = units.alt(n.attribute("x").value());
        double y0 = units.alt(n.attribute("y").value());
        double z0 = units.alt(n.attribute("z").value());

        double Rx,Ry;
        if ( n.attribute("R").empty() ){
           Rx = units.alt(n.attribute("Rx").value());
           Ry = units.alt(n.attribute("Ry").value());
        } else {
           double R = units.alt(n.attribute("R").value());
           Rx = R;
           Ry = R;
        }

        double Rx2 = Rx*Rx;
        double Ry2 = Ry*Ry;
   
        reg.dx = x0 - Rx - 5;
        reg.dy = y0 - Ry - 5;

        reg.nx = 2*Rx + 10;
        reg.ny = 2*Ry + 10;
	    debug1("Filling Z-pipe  with flag %d (%d)\n, with center at (%lf,%lfi,%f) and radius (%lf,%lf)", fg, fg_mask, x0, y0, z0, Rx,Ry);
	    DEBUG1(reg.print();)
		for (int x = reg.dx; x < reg.dx + reg.nx; x++)
		for (int y = reg.dy; y < reg.dy + reg.ny; y++)
		    for (int z = reg.dz; z < reg.dz + reg.nz; z++) {
               double xx =  (.5 + x - x0);  
               double yy =  (.5 + y - y0);  
			   if ( xx*xx/Rx2 + yy*yy/Ry2 < 1. ) {
			    Dot(x, y, z);
			   }
		}
	} else if (strcmp(n.name(),"XPipe") == 0) {

	double x0 = units.alt(n.attribute("x").value());
	double y0 = units.alt(n.attribute("y").value());
	double z0 = units.alt(n.attribute("z").value());

	double Ry, Rz;
	if ( n.attribute("R").empty() ) {
	    Ry = units.alt(n.attribute("Ry").value());
	    Rz = units.alt(n.attribute("Rz").value());
	} else {
	    double R = units.alt(n.attribute("R").value());
	    Ry = R;
	    Rz = R;
	}

	double Ry2 = Ry*Ry;
	double Rz2 = Rz*Rz;

	reg.dy = y0 - Ry - 5;
	reg.dz = z0 - Rz - 5;

	reg.ny = 2*Ry + 10;
	reg.nz = 2*Rz + 10;

	    debug1("Filling XPipe with flag %d (%d)\n, with center at (%lf,%lfi,%f) radius (%lf,%lf)", fg, fg_mask, x0, y0, z0, Ry, Rz);
	    DEBUG1(reg.print();)
		for (int y = reg.dy; y < reg.dy + reg.ny; y++)
		for (int z = reg.dz; z < reg.dz + reg.nz; z++)
       		    for (int x = reg.dx; x < reg.dx + reg.nx; x++) {
		double yy = (.5 + y - y0);
		double zz = (.5 + z - z0);
			if (zz*zz/Rz2 + yy*yy/Ry2 < 1. ) {
			    Dot(x,y,z);
			}
		}
	} else if (strcmp(n.name(),"YPipe") == 0) {

	double x0 = units.alt(n.attribute("x").value());
	double y0 = units.alt(n.attribute("y").value());
	double z0 = units.alt(n.attribute("z").value());

	double Rx, Rz;
	if ( n.attribute("R").empty() ) {
	    Rx = units.alt(n.attribute("Rx").value());
	    Rz = units.alt(n.attribute("Rz").value());
	} else {
	    double R = units.alt(n.attribute("R").value());
	    Rx = R;
	    Rz = R;
	}

	double Rx2 = Rx*Rx;
	double Rz2 = Rz*Rz;

	reg.dx = x0 - Rx - 5;
	reg.dz = z0 - Rz - 5;

	reg.nx = 2*Rx + 10;
	reg.nz = 2*Rz + 10;

	    debug1("Filling YPipe with flag %d (%d)\n, with center at (%lf,%lfi,%f) radius (%lf,%lf)", fg, fg_mask, x0, y0, z0, Rx, Rz);
	    DEBUG1(reg.print();)
		for (int y = reg.dy; y < reg.dy + reg.ny; y++)
		for (int z = reg.dz; z < reg.dz + reg.nz; z++)
       		    for (int x = reg.dx; x < reg.dx + reg.nx; x++) {
		double xx = (.5 + x - y0);
		double zz = (.5 + z - z0);
			if (zz*zz/Rz2 + xx*xx/Rx2 < 1. ) {
			    Dot(x,y,z);
			}
		}
	} else if (strcmp(n.name(),"ZPipe") == 0) {

	double x0 = units.alt(n.attribute("x").value());
	double y0 = units.alt(n.attribute("y").value());
	double z0 = units.alt(n.attribute("z").value());

	double Rx, Ry;
	if ( n.attribute("R").empty() ) {
	    Rx = units.alt(n.attribute("Rx").value());
	    Ry = units.alt(n.attribute("Ry").value());
	} else {
	    double R = units.alt(n.attribute("R").value());
	    Rx = R;
	    Ry = R;
	}

	double Rx2 = Rx*Rx;
	double Ry2 = Ry*Ry;

	reg.dx = x0 - Rx - 5;
	reg.dy = y0 - Ry - 5;

	reg.nx = 2*Rx + 10;
	reg.ny = 2*Ry + 10;

	    debug1("Filling YPipe with flag %d (%d)\n, with center at (%lf,%lfi,%f) radius (%lf,%lf)", fg, fg_mask, x0, y0, z0, Rx, Ry);
	    DEBUG1(reg.print();)
		for (int y = reg.dy; y < reg.dy + reg.ny; y++)
		for (int z = reg.dz; z < reg.dz + reg.nz; z++)
       		    for (int x = reg.dx; x < reg.dx + reg.nx; x++) {
		double xx = (.5 + x - y0);
		double yy = (.5 + y - y0);
			if (yy*yy/Ry2 + xx*xx/Rx2 < 1. ) {
			    Dot(x,y,z);
			}
		}
	} else if (strcmp(n.name(),"XAnnulus") == 0) {

	double x0 = units.alt(n.attribute("x").value());
	double y0 = units.alt(n.attribute("y").value());
	double z0 = units.alt(n.attribute("z").value());

	double Ry_o, Rz_o;
	double Ry_i, Rz_i;
	if ( n.attribute("Ro").empty() ) {
	    Ry_i = units.alt(n.attribute("Ry_i").value());
	    Rz_i = units.alt(n.attribute("Rz_i").value());
	    Ry_o = units.alt(n.attribute("Ry_o").value());
	    Rz_o = units.alt(n.attribute("Rz_o").value());
	} else {
	    double Ro = units.alt(n.attribute("Ro").value()); //Outer annular radius
	    double Ri = units.alt(n.attribute("Ri").value()); //Inner annular radius
	    Ry_o = Ro;
	    Rz_o = Ro;
	    Ry_i = Ri;
	    Rz_i = Ri;
	}

	double Ry2_o = Ry_o*Ry_o;
	double Rz2_o = Rz_o*Rz_o;
	double Ry2_i = Ry_i*Ry_i;
	double Rz2_i = Rz_i*Rz_i;

	reg.dy = y0 - Ry_o - 5;
	reg.dz = z0 - Rz_o - 5;

	reg.ny = 2*Ry_o + 10;
	reg.nz = 2*Rz_o + 10;

	    debug1("Filling XAnnulus with flag %d (%d)\n, with center at (%lf,%lf,%lf) inner radius (%lf,%lf) and outer radius (%lf,%lf)", fg, fg_mask, x0, y0, z0, Ry_i, Rz_i, Ry_o, Rz_o);
	    DEBUG1(reg.print();)
		for (int y = reg.dy; y < reg.dy + reg.ny; y++)
		for (int z = reg.dz; z < reg.dz + reg.nz; z++)
       		    for (int x = reg.dx; x < reg.dx + reg.nx; x++) {
		double yy = (.5 + y - y0);
		double zz = (.5 + z - z0);
			if ( (zz*zz/Rz2_o + yy*yy/Ry2_o < 1.) && (zz*zz/Rz2_i + yy*yy/Ry2_i > 1. ) ) {
			    Dot(x,y,z);
			}
		}
	} else if (strcmp(n.name(),"Pipe") == 0) {
	    debug1("Filling pipe with flag %d (&d)\n",fg,fg_mask);
	    DEBUG1(reg.print();)
        for (int x = reg.dx; x < reg.dx + reg.nx; x++)
            for (int y = reg.dy - 1 ; y < reg.dy + reg.ny + 1; y++)
                for (int z = reg.dz - 1; z < reg.dz + reg.nz + 1;z++) {
                    if (!inSphere(0.5, (.5 + y - reg.dy) / reg.ny, (.5 + z - reg.dz) / reg.nz)) {
				        Dot(x,y,z);
                    }
                }
  } else if (strcmp(n.name(),"PipeY") == 0) {
	    debug1("Filling pipe with flag %d (&d)\n",fg,fg_mask);
	    DEBUG1(reg.print();)
      for (int x = reg.dx - 1; x < reg.dx + reg.nx + 1; x++)
		      for (int y = reg.dy ; y < reg.dy + reg.ny; y++)
		          for (int z = reg.dz - 1; z < reg.dz + reg.nz + 1;z++) {
			            if (!inSphere(0.5, (.5 + x - reg.dx) / reg.nx, (.5 + z - reg.dz) / reg.nz)) {
				              Dot(x,y,z);
			            }
		          }
	} else if (strcmp(n.name(),"PipeZ") == 0) {
	    debug1("Filling Z-pipe with flag %d (&d)\n",fg,fg_mask);
	    DEBUG1(reg.print();)
        for (int z = reg.dz; z < reg.dz + reg.nz; z++)
            for (int y = reg.dy - 1 ; y < reg.dy + reg.ny + 1; y++)
                for (int x = reg.dx - 1; x < reg.dx + reg.nx + 1;x++) {
                    if (!inSphere(0.5, (.5 + y - reg.dy) / reg.ny, (.5 + x - reg.dx) / reg.nx)) {
				        Dot(x,y,z);
                    }
                }
    }
    else if (strcmp(n.name(),"Cylinder") == 0) {
        debug1("Filling cylinder with flag %d (&d)\n",fg,fg_mask);
        DEBUG1(reg.print();)
        for (int x = reg.dx; x < reg.dx + reg.nx; x++)
            for (int y = reg.dy - 1 ; y < reg.dy + reg.ny + 1; y++)
                for (int z = reg.dz - 1; z < reg.dz + reg.nz + 1;z++) {
                    if (inSphere((.5 + x - reg.dx) / reg.nx, (.5 + y - reg.dy) / reg.ny, 0.5)) {
                        Dot(x,y,z);
                    }
                }
    }
    else if (strcmp(n.name(), "Wedge") == 0) {
//                      reg = region.intersect(reg);
	    debug1("Filling wedge with flag %d (%d)\n", fg, fg_mask);
	    DEBUG1(reg.print();)
	    const char *type = n.attribute("direction").value();
	    for (int x = reg.dx; x < reg.dx + reg.nx; x++)
		for (int y = reg.dy; y < reg.dy + reg.ny; y++)
		    for (int z = reg.dz; z < reg.dz + reg.nz; z++) {
			if (inWedge((x - reg.dx) / (reg.nx - 1.), (y - reg.dy) / (reg.ny - 1.), (z - reg.dz) / (reg.nz - 1.), type)) {
			    Dot(x, y, z);
			}
         }
    } else if (strcmp(n.name(), "STL") == 0) {
	    debug1("Filling stl geometry with flag %d (%d)\n", fg, fg_mask);
	    DEBUG1(reg.print();)
		if (loadSTL(reg, n))
		return -1;
	} else if (strcmp(n.name(), "Sweep") == 0) {
		debug1("Filling a sweep geometry with flag %d (%d)\n", fg, fg_mask);
		DEBUG1(reg.print();)
		if (loadSweep(reg, n))
		return -1;
	} else if (strcmp(n.name(), "Text") == 0) {
	    lbRegion crop = getRegion(n.parent());
	    crop = region.intersect(crop);
	    // crop.print();
	    if (!n.attribute("file")) {
		error("No 'file' attribute in 'Text' element in xml conf\n");
		return -1;
	    }
	    FILE *f = fopen(n.attribute("file").value(), "rt");
	    if (f == NULL) {
		error("Could not open file: %s\n", n.attribute("file").value());
		return -1;
	    }
	    output("Reading file %s\n", n.attribute("file").value());
	    int p[3], dp[3], np[3], xp=-1, yp=-1, zp=-1;
	    dp[0] = 0; np[0] = 1; dp[1] = 0; np[1] = 1; dp[2] = 0; np[2] = 1;
	    if (n.attribute("order")) {
	    	const char * ord = n.attribute("order").value();
	    	int len = strlen(ord);
	    	if (len != 3) {
	    		ERROR("order attribute in Text has to have 3 characters\n");
	    		return -1;
		}
	    	for (int k=0;k<len;k++) {
	    		if (ord[k] == 'x' && xp == -1) {
	    			xp = k;
			} else if (ord[k] == 'y' && yp == -1) {
	    			yp = k;
			} else if (ord[k] == 'z' && zp == -1) {
	    			zp = k;
			} else {
		    		ERROR("wrong characters in order attribute in Text: '%s'\n", ord);
		    		return -1;
			}
		}
	    } else {
	    	xp = 0;
	    	yp = 1;
		zp = 2;
	    }
	    dp[xp] = reg.dx;
	    np[xp] = reg.nx;
	    dp[yp] = reg.dy;
	    np[yp] = reg.ny;
	    dp[zp] = reg.dz;
	    np[zp] = reg.nz;
	    for (p[0] = dp[0]; p[0] < dp[0]+np[0]; p[0]++)
	        for (p[1] = dp[1]; p[1] < dp[1]+np[1]; p[1]++)
		    for (p[2] = dp[2]; p[2] < dp[2]+np[2]; p[2]++) {
		    	int x = p[xp];
		    	int y = p[yp];
		    	int z = p[zp];
			int v;
			int ret = fscanf(f, "%d", &v);
                        if (ret != 1) {
                            ERROR("File (%s) ended while reading\n", n.attribute("file").value());
                            return -1;
                        }
			if ((v != 0) && (crop.isIn(x, y, z)))
			    Dot(x, y, z);
		    }
	    fclose(f);
	} else {
	    pugi::xml_node node = fg_xml.find_child_by_attribute("Zone", "name", n.name());
	    if (node) {
		E(Draw(node));
	    } else {
		error("Unknown geometry element: %s\n", n.name());
		return -1;
	    }
	}
    }
    return 0;
}

/// Loades Zone definition
int Geometry::loadZone(const char *name)
{
    pugi::xml_node node = fg_xml.find_child_by_attribute("Zone", "name", name);
    if (!node) {
        error("Unknown zone (in xml): %s", name);
        return -1;
    }
    E(Draw(node));
    return 0;
}

pugi::xml_document& getXMLDef() {
  static pugi::xml_document xml_def;
  static bool dummy_static_init = [&] () -> bool {
    const char* xml_definition = "<Geometry>\
    	<Zone name='Inlet'> <Box dx='0' dy='0' dz='0' fx='0' fy='-1' fz='-1'/></Zone>\
    	<Zone name='Outlet'> <Box dx='-1' dy='0' dz='0' fx='-1' fy='-1' fz='-1'/></Zone>\
    	<Zone name='Channel'>\
    		<Box dx='0' dy='0' dz='0' fx='-1' fy='0' fz='-1'/>\
    		<Box dx='0' dy='-1' dz='0' fx='-1' fy='-1' fz='-1'/>\
    	</Zone>\
    	<Zone name='Tunnel'>\
    		<Box dx='0' dy='0' dz='0' fx='-1' fy='0' fz='-1'/>\
    		<Box dx='0' dy='-1' dz='0' fx='-1' fy='-1' fz='-1'/>\
    		<Box dx='0' dy='0' dz='0' fx='-1' fy='-1' fz='0'/>\
    		<Box dx='0' dy='0' dz='-1' fx='-1' fy='-1' fz='-1'/>\
    	</Zone>\
    	<Zone name='Tunnel'> <Box dx='0' dy='0' dz='0' fx='0' fy='-1' fz='-1'/></Zone>\
    	<Zone name='Inlet'><Box dx='0' dy='0' dz='0' fx='0' fy='-1' fz='-1'/></Zone>\
    </Geometry>";
    const auto init_status = xml_def.load_string(xml_definition);
    if(!init_status) {
      ERROR("Error while parsing in-program default settings xml: %s\n", init_status.description());
      std::terminate();
    }
    return true;
  }();
  return xml_def;
}

/// Loads Geometry from a XML tree
int Geometry::load(pugi::xml_node& node, const std::map<std::string, int>& zone_map)
{
    using namespace std::string_view_literals;
	output("Loading geometry...");
    pugi::xml_node geom_def = getXMLDef().child("Geometry");
    fg_xml = node;
    for (pugi::xml_node z = geom_def.first_child(); z; z = z.next_sibling()) {
        pugi::xml_attribute attr = z.attribute("name");
        if (!attr) continue;
        if (node.find_child_by_attribute(z.name(), "name", attr.value())) continue;
        node.prepend_copy(z);
    }
    for (pugi::xml_node n = node.first_child(); n; n = n.next_sibling()) {
        if(n.name() == "Zone"sv || n.name() == "Type"sv || n.name() == "Mask"sv) continue;
        E(setFlag(n.name()));
        for (pugi::xml_attribute attr = n.first_attribute(); attr; attr = attr.next_attribute()) {
	        if (attr.name() == "name"sv) {
	            const int zone_number = zone_map.at(std::string(attr.value()));
                setZone(zone_number);
            } else if (attr.name() == "mask"sv) {
                E(setMask(attr.value()));
            } else if (attr.name() == "mode"sv) {
                E(setMode(attr.value()));
            }
        }
        if (auto attr = n.attribute("zone"); attr) {
            loadZone(attr.value());
        }
        E(Draw(n));
    }
    if (node.attribute("save"))
        writeVTI(node.attribute("save").value());
    return 0;
}

Geometry::~Geometry()
{
    debug1("[%d] Destroy geom\n", D_MPI_RANK);
    if (Q != NULL) delete[] Q;
}


/// Dumps the Geometry to a vti file
// use to check values of cuts in the interpolated bounce back scheme
// <Geometry nx="32" ny="32"  nz="3" save="geometry_ibb_output">  // output path goues here
//      <BGK><Box/></BGK>
// 		<IBB name="border" >
// 			<STL file="path_to_geometry_file.stl" scale="1" Xrot="0d" x="0" y="0" z="0" side="surface"/>
// 		</IBB>
// </Geometry>
void Geometry::writeVTI(const char *name)
{
    vtkFileOut vtkFile(MPMD.local);
    char filename[STRING_LEN];
    sprintf(filename, "%s_P%02d.vti", name, D_MPI_RANK);
    if (vtkFile.Open(filename)) {
	return;
    }
    vtkFile.Init(region, "");
    vtkFile.WriteField("geom", geom.data());
    if (Q != NULL) {
        size_t regsize = region.sizeL();

		real_t *  Qnorm = new real_t[regsize*26];

		// src/types.h.Rt:19:  #define NO_CUT 65535
		// src/types.h.Rt:20:  #define CUT_MAX 65000
		// src/types.h.Rt:17:  typedef unsigned short int cut_t; - contains at least [0, 65535]

		for (size_t i = 0; i < regsize*26; ++i)  // normalize the cuts to 0-1 range
		{
			Qnorm[i]=real_t(Q[i])/CUT_MAX;
			// if (Qnorm[i] > 1.)
			// 	Qnorm[i] = 0; // means no cut
		}
        for (int d = 0; d < 26; d++) {
            char nm[20];
			sprintf(nm,"Qnorm%d%d%d",d3q27_vec[(d+1)*3+0],d3q27_vec[(d+1)*3+1],d3q27_vec[(d+1)*3+2]);
			// sprintf(nm,"Q_%d",d);
            // vtkFile.WriteField(nm, Q + regsize*d);
			vtkFile.WriteField(nm, Qnorm + regsize*d);
        }
	 	delete[] Qnorm;

		for (int d = 0 ; d < 26; d++) {
            char nm[20];
			sprintf(nm,"Q%d%d%d",d3q27_vec[(d+1)*3+0],d3q27_vec[(d+1)*3+1],d3q27_vec[(d+1)*3+2]);
			// sprintf(nm,"Q_%d",d);
            vtkFile.WriteField(nm, Q + regsize*d);
        }
    }
    vtkFile.Finish();
    vtkFile.Close();
}
