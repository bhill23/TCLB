# Density - table of variables of LB Node to stream
#	Velocity-based Evolution d3q27:
if (Options$q27){
	source("d3q27q27.R")
} else {
	source("d3q27q15.R")
}

AddDensity(name="pnorm", dx=0, dy=0, dz=0, group="Vel")
AddDensity(name="U", dx=0, dy=0, dz=0, group="Vel")
AddDensity(name="V", dx=0, dy=0, dz=0, group="Vel")
AddDensity(name="W", dx=0, dy=0, dz=0, group="Vel")

AddDensity(name="nw_x", dx=0, dy=0, dz=0, group="nw")
AddDensity(name="nw_y", dx=0, dy=0, dz=0, group="nw")
AddDensity(name="nw_z", dx=0, dy=0, dz=0, group="nw")

save_initial_PF = c("PF")
save_initial    = c("g","h","Vel","PF")
save_iteration  = c("g","h","Vel","nw")
load_iteration  = c("g","h","Vel","nw")
load_phase      = c("g","h","Vel","nw")

if (Options$altContactAngle){
    AddDensity(name="n_k", dx=0, dy=0, dz=0, group="nw")
    AddDensity(name="der_tangent_1_wall", dx=0, dy=0, dz=0)
    AddDensity(name="der_tangent_2_wall", dx=0, dy=0, dz=0)
    # Debugging
    AddDensity(name="perpVal", dx=0, dy=0, dz=0)
    AddField("gradPhiVal_x", stencil3d=2, group="gradPhi")
    AddField("gradPhiVal_y", stencil3d=2, group="gradPhi")
    AddField("gradPhiVal_z", stencil3d=2, group="gradPhi")
    AddField("IsBoundary", stencil3d=1, group="debug_boundary")

    AddDensity(name="TangentWallVector1_x", dx=0, dy=0, dz=0)
    AddDensity(name="TangentWallVector2_x", dx=0, dy=0, dz=0)
    AddDensity(name="TangentWallVector1_y", dx=0, dy=0, dz=0)
    AddDensity(name="TangentWallVector2_y", dx=0, dy=0, dz=0)
    AddDensity(name="TangentWallVector1_z", dx=0, dy=0, dz=0)
    AddDensity(name="TangentWallVector2_z", dx=0, dy=0, dz=0)
    AddField("PhaseF",stencil3d=3, group="PF")

    AddStage("WallInit_CA"  , "Init_wallNorm", save=Fields$group %in% c("nw", "debug_boundary"))
    AddStage("calcWall_CA"  , "calcWallPhase", save=Fields$name %in% c("PhaseF", "der_tangent_1_wall", "der_tangent_2_wall", "TangentWallVector1_x", "TangentWallVector2_x", "TangentWallVector1_y", "TangentWallVector2_y", "TangentWallVector1_z", "TangentWallVector2_z", "perpVal"), load=DensityAll$group %in% c("nw", "gradPhi", "PF"))

    AddStage('calcPhaseGrad', "calcPhaseGrad", load=DensityAll$group %in% c("g","h","Vel","nw", "PF"), save=Fields$group=="gradPhi")
    AddStage('calcPhaseGrad_init', "calcPhaseGrad_init", load=DensityAll$group %in% c("g","h","Vel","nw", "PF"), save=Fields$group=="gradPhi")
} else {
    AddField("PhaseF",stencil3d=1, group="PF")
}


AddDensity(name="Iterations", dx=0, dy=0, dz=0, group="Vel")
if (Options$HBF){
	AddDensity(name="StrainRate", dx=0, dy=0, dz=0, group="HBF")
	# for (D in c('Dxx','Dxy','Dxz','Dyy','Dyz','Dzz')){
	# 	AddDensity(name=D, dx=0, dy=0, dz=0, group="HBF")
	# }

	# Don't need to explicitly keep Tau, StrainRate is sufficent with a bit extra work
	# AddDensity(name="Tau", dx=0, dy=0, dz=0, group="HBF")

	# AddDensity(name="nu0_l", dx=0, dy=0, dz=0, group="HBF")
	# AddDensity(name="nu0_h", dx=0, dy=0, dz=0, group="HBF")
	# AddDensity(name="nuc_l", dx=0, dy=0, dz=0, group="HBF")
	# AddDensity(name="nuc_h", dx=0, dy=0, dz=0, group="HBF")

	# AddSetting(name="nu0_l", comment='Do Not Set')#, nu0_l='Consistency_index_l*Density_l * pow(Reg_m,1.0-n_l) + Reg_m * Yield_stress_l'
	# AddSetting(name="nu0_h", comment='Do Not Set')#, nu0_h='Consistency_index_h*Density_h * pow(Reg_m,1.0-n_h) + Reg_m * Yield_stress_h'
	# AddSetting(name="nuc_l", comment='Do Not Set')#, nuc_l='Yield_stress_l/strainLimit * (1.0 - exp(-Reg_m*strainLimit)) + Consistency_index_l*Density_l * pow(strainLimit/(1.0 - exp(-Reg_m*strainLimit)),n_l-1.0)'
	# AddSetting(name="nuc_h", comment='Do Not Set')#, nuc_h='Yield_stress_h/strainLimit * (1.0 - exp(-Reg_m*strainLimit)) + Consistency_index_h*Density_h * pow(strainLimit/(1.0 - exp(-Reg_m*strainLimit)),n_h-1.0)'

	save_initial   = c(save_initial,  "HBF")
    save_iteration = c(save_iteration,"HBF")
    load_iteration = c(load_iteration,"HBF")
    load_phase     = c(load_phase,    "HBF")

}



if (Options$OutFlow){
	for (d in rows(DensityAll)) {
		AddField( name=d$name, dx=-d$dx-1, dy=-d$dy, dz=-d$dz )
		AddField( name=d$name, dx=-d$dx+1, dy=-d$dy, dz=-d$dz )
	}
	AddField(name="U",dx=c(-1,0,0))
	AddField(name="U",dx=c(1,0,0))

    save_initial   = c(save_initial,  "gold","hold")
    save_iteration = c(save_iteration,"gold","hold")
    load_iteration = c(load_iteration,"gold","hold")
    load_phase     = c(load_phase,    "gold","hold")
}
###############################
########THERMOCAPILLARY########
###############################
if (Options$thermo){
    source("thermocapillary.R")

    save_initial_PF = c(save_initial_PF,"Thermal")
    save_iteration  = c(save_iteration, "Thermal")
    load_iteration  = c(load_iteration, "Thermal")
}
######################
########STAGES########
######################
AddStage("WallInit" , "Init_wallNorm", save=Fields$group=="nw")
AddStage("calcWall" , "calcWallPhase", save=Fields$name=="PhaseF", load=DensityAll$group=="nw")
AddStage("PhaseInit", "Init", save=Fields$group %in% save_initial_PF)
AddStage("BaseInit" , "Init_distributions", save=Fields$group %in% save_initial)
AddStage("calcPhase", "calcPhaseF", save=Fields$name=="PhaseF", load=DensityAll$group %in% load_phase)
AddStage("BaseIter" , "Run", save=Fields$group %in% save_iteration, load=DensityAll$group %in% load_iteration )
#######################
########ACTIONS########
#######################
	if (Options$thermo){	
		AddAction("TempToSteadyState", c("CopyDistributions","RK_1", "RK_2", "RK_3", "RK_4","NonLocalTemp"))
		AddAction("Iteration", c("BaseIter", "calcPhase", "calcWall","RK_1", "RK_2", "RK_3", "RK_4","NonLocalTemp"))
		AddAction("IterationConstantTemp", c("BaseIter", "calcPhase", "calcWall","CopyThermal"))
		AddAction("Init"     , c("PhaseInit","WallInit" , "calcWall","BaseInit"))
	} else if (Options$altContactAngle) {
        AddAction("Iteration", c("BaseIter", "calcPhase",    "calcPhaseGrad", "calcWall_CA"))
	    AddAction("Init"     , c("PhaseInit","WallInit_CA" , "calcPhaseGrad_init", "calcWall_CA","BaseInit"))
    } else {
		AddAction("Iteration", c("BaseIter", "calcPhase", "calcWall"))
		AddAction("Init"     , c("PhaseInit","WallInit" , "calcWall","BaseInit"))
	}
#######################
########OUTPUTS########
#######################
	AddQuantity(name="Rho",unit="kg/m3")
	AddQuantity(name="PhaseField",unit="1")
	AddQuantity(name="U",	  unit="m/s",vector=T)
	AddQuantity(name="P",	  unit="Pa")
	AddQuantity(name="Pstar", unit="1")
	AddQuantity(name="Normal", unit=1, vector=T)
if (Options$altContactAngle){
    AddQuantity(name="TangentWallVector1", unit=1, vector=T)
    AddQuantity(name="TangentWallVector2", unit=1, vector=T)
    AddQuantity(name="Tangent1Wall", unit="1")
    AddQuantity(name="Tangent2Wall", unit="1")
    AddQuantity(name="GradPhi", unit=1, vector=T)
    AddQuantity(name="PerpVal", unit="1")
    AddQuantity(name="IsItBoundary", unit="1")
}
	AddQuantity(name="Iterations",unit="1")
if (Options$HBF){
	AddQuantity(name="StrainRate", unit="1/s")
	# for (D in c('Dxx','Dxy','Dxz','Dyy','Dyz','Dzz')){
	# 	AddQuantity(name=D, unit="1")
	# }
	AddQuantity(name="Viscosity",unit="m2/s")
}

###################################
########INPUTS - PHASEFIELD########
###################################
	AddSetting(name="Density_h", comment='High density')
	AddSetting(name="Density_l", comment='Low  density')
	AddSetting(name="PhaseField_h", default=1, comment='PhaseField in Liquid')
	AddSetting(name="PhaseField_l", default=0, comment='PhaseField gas')
	AddSetting(name="PhaseField", 	   comment='Initial PhaseField distribution', zonal=T)
	AddSetting(name="IntWidth", default=4,    comment='Anti-diffusivity coeff')
	AddSetting(name="Interpolation_type", default=2, comment='+ve for kinematic, -ve for dynamic. abs value: 1-sharp, 2-linear, 3-inverse')
	AddSetting(name="omega_phi", comment='one over relaxation time (phase field)')
	AddSetting(name="M", omega_phi='1.0/(3*M+0.5)', default=0.02, comment='Mobility')
	AddSetting(name="sigma", comment='surface tension')
  AddSetting(name="Washburn_start", default="0", comment='Start of washburn gas phase')
  AddSetting(name="Washburn_end", default="0", comment='End of washburn gas phase')
	AddSetting(name="radAngle", default='1.570796', comment='Contact angle in radians, can use units -> 90d where d=2pi/360', zonal=T)
	AddSetting(name="minGradient", default='1e-8', comment='if the phase gradient is less than this, set phase normals to zero')
	##SPECIAL INITIALISATIONS
	# RTI
		AddSetting(name="RTI_Characteristic_Length", default=-999, comment='Use for RTI instability')
	    AddSetting(name="pseudo2D", default="0", comment="if 1, assume model is pseduo2D")
    # Single droplet/bubble
		AddSetting(name="Radius", default=0.0, comment='Diffuse Sphere Radius')
		AddSetting(name="CenterX", default=0.0, comment='Diffuse sphere center_x')
		AddSetting(name="CenterY", default=0.0, comment='Diffuse sphere center_y')
		AddSetting(name="CenterZ", default=0.0, comment='Diffuse sphere center_z')
		AddSetting(name="BubbleType",default=1.0, comment='droplet(1.0) or bubble(-1.0)?!')
	# Annular Taylor bubble
		AddSetting(name="DonutTime", default=0.0, comment='Radius of a Torus - initialised to travel along x-axis')
		AddSetting(name="Donut_h",   default=0.0, comment='Half donut thickness, i.e. the radius of the cross-section')
		AddSetting(name="Donut_D",   default=0.0, comment='Dilation factor along the x-axis')
		AddSetting(name="Donut_x0",  default=0.0, comment='Position along x-axis')
	# Poiseuille flow in 2D channel (flow in x direction)
		AddSetting("HEIGHT", default=0,	comment="Height of channel for 2D Poiseuille flow")
		AddSetting("Uavg", default=0,	zonal=T, comment="Average velocity of channel for 2D Poiseuille flow")
		AddSetting("developedFlow", default=0,	comment="set greater than 0 for fully developed flow in the domain (x-direction)")
		AddSetting("developedPipeFlow", default=0,	comment="set greater than 0 for fully developed pipe flow in the inlets")
		AddSetting("developedPipeFlow_X", default=0,comment="set greater than 0 for fully developed pipe flow in the domain (x-direction-only)")
        AddSetting("pipeRadius", default=0, comment="radius of pipe for developed pipe flow")
        AddSetting("pipeCentre_Y", default=0, comment="pipe centre Y co-ord for developed pipe flow")
        AddSetting("pipeCentre_Z", default=0, comment="pipe centre Z co-ord for developed pipe flow")
##############################
########INPUTS - FLUID########
##############################

if (Options$HBF){
	AddSetting(name="Viscosity_l", Consistency_index_l='Viscosity_l', comment='kinematic viscosity')
	AddSetting(name="Viscosity_h", Consistency_index_h='Viscosity_h', comment='kinematic viscosity')
	AddSetting(name="Consistency_index_l", default=0.16666666, comment='kinematic power-law coefficient')
	AddSetting(name="Consistency_index_h", default=0.16666666, comment='kinematic power-law coefficient')
	AddSetting(name="n_l", default=1.0, comment='power law index')
	AddSetting(name="n_h", default=1.0, comment='power law index')
	AddSetting(name='Yield_stress_l', default=0.0, comment='Yield Stress')
	AddSetting(name='Yield_stress_h', default=0.0, comment='Yield Stress')
	
	AddSetting(name='Reg_m', default=1e10, comment='Regularisation Parameter')
	AddSetting(name='strainLimit', default=1e-12, comment='highest strain rate at which interpolation is used')
} else {
	# AddSetting(name="tau_l", comment='relaxation time (low density fluid)')
	# AddSetting(name="tau_h", comment='relaxation time (high density fluid)')
	AddSetting(name="Viscosity_l", default=0.16666666, comment='kinematic viscosity')
	AddSetting(name="Viscosity_h", default=0.16666666, comment='kinematic viscosity')
	AddSetting(name="Consistency_index_l", Viscosity_l='Consistency_index_l', comment='convenience setting for viscosity')
	AddSetting(name="Consistency_index_h", Viscosity_h='Consistency_index_h', comment='convenience setting for viscosity')
}
	
	
	#	Inputs: Flow Properties
	AddSetting(name="VelocityX", default=0.0, comment='inlet/outlet/init velocity', zonal=T)
	AddSetting(name="VelocityY", default=0.0, comment='inlet/outlet/init velocity', zonal=T)
	AddSetting(name="VelocityZ", default=0.0, comment='inlet/outlet/init velocity', zonal=T)
	AddSetting(name="Pressure" , default=0.0, comment='inlet/outlet/init density', zonal=T)
	AddSetting(name="GravitationX", default=0.0, comment='applied (rho)*GravitationX')
	AddSetting(name="GravitationY", default=0.0, comment='applied (rho)*GravitationY')
	AddSetting(name="GravitationZ", default=0.0, comment='applied (rho)*GravitationZ')
	AddSetting(name="BuoyancyX", default=0.0, comment='applied (rho_h-rho)*BuoyancyX')
	AddSetting(name="BuoyancyY", default=0.0, comment='applied (rho_h-rho)*BuoyancyY')
	AddSetting(name="BuoyancyZ", default=0.0, comment='applied (rho_h-rho)*BuoyancyZ')
	AddSetting(name="fixedIterator", default=10.0, comment='fixed iterator for velocity/viscosity calculation')

##################################
########TRACKING VARIABLES########
##################################
	AddSetting(name="xyzTrack", default=1,comment='x<-1, y<-2, z<-3')
	AddNodeType("Centerline",group="ADDITIONALS")
	AddNodeType(name="Spiketrack", group="ADDITIONALS")
	AddNodeType(name="Saddletrack", group="ADDITIONALS")
	AddNodeType(name="Bubbletrack", group="ADDITIONALS")
	AddGlobal("InterfacePosition0", op="MAX", comment='trackPosition',unit="m")
	AddGlobal("InterfacePosition1", op="MAX", comment='trackPosition',unit="m")
	AddGlobal("Vfront",comment='velocity infront of bubble',unit="m/s")
	AddGlobal("Vback",comment='velocity behind bubble',unit="m/s")
	AddGlobal("RTISpike", op="MAX", comment='SpikeTracker ',unit="m")
	AddGlobal("RTIBubble",op="MAX", comment='BubbleTracker',unit="m")
	AddGlobal("RTISaddle",op="MAX", comment='SaddleTracker',unit="m")
	AddGlobal("XLocation", comment='tracking of x-centroid of the gas regions in domain', unit="m")
	# AddGlobal(name="DropFront",	op="MAX",  comment='Highest location of droplet', unit="m")
	AddNodeType(name="LogP", group="ADDITIONALS")
	AddGlobal(name="VelMag", comment='Velocity magnitude for steady state determination', unit="m/s")
##########################
########NODE TYPES########
##########################
	AddNodeType("Smoothing",group="ADDITIONALS")
	AddNodeType(name="flux_nodes", group="ADDITIONALS")
	dotR_my_velocity_boundaries = paste0(c("N","E","S","W","F","B"),"Velocity")
    dotR_my_pressure_boundaries = paste0(c("N","E","S","W","F","B"),"Pressure")
    for (ii in 1:6){
        AddNodeType(name=dotR_my_velocity_boundaries[ii], group="BOUNDARY")
        AddNodeType(name=dotR_my_pressure_boundaries[ii], group="BOUNDARY")
    }
	AddNodeType(name="MovingWall_N", group="BOUNDARY")
	AddNodeType(name="MovingWall_S", group="BOUNDARY")
	AddNodeType(name="Solid", group="BOUNDARY")
	AddNodeType(name="Wall", group="BOUNDARY")
	AddNodeType(name="BGK", group="COLLISION")
	AddNodeType(name="MRT", group="COLLISION")
	if (Options$OutFlow){
		AddNodeType(name="ENeumann", group="BOUNDARY")
		AddNodeType(name="WNeumann", group="BOUNDARY")
		AddNodeType(name="EConvect", group="BOUNDARY")
		AddNodeType(name="WConvect", group="BOUNDARY")
	}
#######################
########GLOBALS########
#######################
	AddGlobal(name="PressureLoss", comment='pressure loss', unit="1mPa")
	AddGlobal(name="OutletFlux", comment='pressure loss', unit="1m2/s")
	AddGlobal(name="InletFlux", comment='pressure loss', unit="1m2/s")
	AddGlobal(name="TotalDensity", comment='Mass conservation check', unit="1kg/m3")
	AddGlobal(name="KineticEnergy",comment='Measure of kinetic energy', unit="J")
	AddGlobal(name="GasTotalVelocity", comment='use to determine avg velocity of bubbles', unit="m/s")
	AddGlobal(name="GasTotalVelocityX", comment='use to determine avg velocity of bubbles', unit="m/s")
	AddGlobal(name="GasTotalVelocityY", comment='use to determine avg velocity of bubbles', unit="m/s")
	AddGlobal(name="GasTotalVelocityZ", comment='use to determine avg velocity of bubbles', unit="m/s")
	AddGlobal(name="GasTotalPhase",	   comment='use in line with GasTotalVelocity to determine average velocity', unit="1")
	AddGlobal(name="LiqTotalVelocity", 	comment='use to determine avg velocity of droplets', unit="m/s")
	AddGlobal(name="LiqTotalVelocityX", comment='use to determine avg velocity of droplets', unit="m/s")
	AddGlobal(name="LiqTotalVelocityY", comment='use to determine avg velocity of droplets', unit="m/s")
	AddGlobal(name="LiqTotalVelocityZ", comment='use to determine avg velocity of droplets', unit="m/s")
	AddGlobal(name="LiqTotalPhase",	   		comment='use in line with LiqTotalVelocity to determine average velocity', unit="1")
	AddGlobal(name="FluxNodeCount",comment='nodes in flux region', unit="1")
	AddGlobal(name="FluxX",comment='flux in x direction for flux_nodes', unit="1")
	AddGlobal(name="FluxY",comment='flux in y direction for flux_nodes', unit="1")
	AddGlobal(name="FluxZ",comment='flux in z direction for flux_nodes', unit="1")
