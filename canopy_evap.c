#include <stdio.h>
#include <stdlib.h>
#include <vicNl.h>

double canopy_evap(layer_data_struct *layer,
                   veg_var_struct *veg_var, 
                   char CALC_EVAP,
                   int veg_class, 
                   int month, 
		   double Wdew,
                   double evap_temp,
                   double dt,
                   double rad,
		   double vpd,
		   double net_short,
		   double air_temp,
                   double ra,
                   double prec,
                   double displacement,
                   double roughness,
                   double ref_height,
		   double elevation,
		   double *depth,
		   double *Wcr,
		   double *Wpwp)
/**********************************************************************
	canopy_evap.c	Dag Lohmann		September 1995

  This routine computes the evaporation, traspiration and throughfall
  of the vegetation types for multi-layered model.

  The value of x, the fraction of precipitation that exceeds the 
  canopy storage capacity, is returned by the subroutine.

  UNITS:	moist (mm)
		evap (mm)
		prec (mm)
		melt (mm)

  VARIABLE TYPE        NAME          UNITS DESCRIPTION
  atmos_data_struct    atmos         N/A   atmospheric forcing data structure
  layer_data_struct   *layer         N/A   soil layer variable structure
  veg_var_struct      *veg_var       N/A   vegetation variable structure
  soil_con_struct      soil_con      N/A   soil parameter structure
  char                 CALC_EVAP     N/A   TRUE = calculate evapotranspiration
  int                  veg_class     N/A   vegetation class index number
  double               evap_temp     C     evaporation temperature
  int                  month         N/A   current month
  global_param_struct  global        N/A   global parameter structure
  double               mu            fract wet (or dry) fraction of grid cell
  double               ra            s/m   aerodynamic resistance
  double               prec          mm    precipitation
  double               displacement  m     displacement height of surface cover
  double               roughness     m     roughness height of surface cover
  double               ref_height    m     measurement reference height

  Modifications:
  9/1/97	Greg O'Donnell
  4-12-98  Code cleaned and final version prepared, KAC

**********************************************************************/
{

  /** declare global variables **/
  extern veg_lib_struct *veg_lib; 
  extern debug_struct debug;
  extern option_struct options;

  /** declare local variables **/
  int    Ndist;
  int    dist;
  int    i;
  double ppt;		/* effective precipitation */
  double f;		/* fraction of time step used to fill canopy */
  double throughfall;
  double Evap;
  double exponent;
  double canopyevap;
  double *layerevap;

  /********************************************************************** 
     CANOPY EVAPORATION

     Calculation of evaporation from the canopy, including the
     possibility of potential evaporation exhausting ppt+canopy storage
     2.16 + 2.17
     Index [0] refers to current time step, index [1] to next one
     If f < 1.0 than veg_var->canopyevap = veg_var->Wdew + ppt and
                     Wdew = 0.0

     DEFINITIONS:
     Wdmax - max monthly dew holding capacity
     Wdew - dew trapped on vegetation

     Modified 4-14-98 to work within calc_surf_energy_balance.c  KAC

  **********************************************************************/ 

  layerevap = (double *)calloc(options.Nlayer,sizeof(double));

  ppt = prec;

  /**************************************************
    Compute Evaporation from Canopy Intercepted Water
    **************************************************/

  /** Due to month changes ..... Wdmax based on LAI **/
  throughfall = 0.0;
  veg_var->Wdew = Wdew;
  if (Wdew > veg_lib[veg_class].Wdmax[month-1]) {
    throughfall  =  Wdew - veg_lib[veg_class].Wdmax[month-1];
    Wdew = veg_lib[veg_class].Wdmax[month-1];
  }

  canopyevap = pow((Wdew / veg_lib[veg_class].Wdmax[month-1]),
               (2.0/3.0))* penman(rad, vpd * 1000., ra, 
               (double) 0.0, veg_lib[veg_class].rarc,
               veg_lib[veg_class].LAI[month-1], 
               (double) 1.0, air_temp, evap_temp, net_short,
               elevation) * dt / 24.0;

  if (canopyevap > 0.0)
    f = min(1.0,((Wdew+ppt) / canopyevap));
  else
    f = 1.0;
  canopyevap *= f;

  Wdew += ppt - canopyevap;
  if (Wdew < 0.0) 
    Wdew = 0.0;
  if (Wdew <= veg_lib[veg_class].Wdmax[month-1]) 
    throughfall += 0.0;
  else {
    throughfall += Wdew - veg_lib[veg_class].Wdmax[month-1];
    Wdew = veg_lib[veg_class].Wdmax[month-1];
  }

  /*******************************************
    Compute Evapotranspiration from Vegetation
    *******************************************/
  if(CALC_EVAP)
    transpiration(layer, veg_class, month, evap_temp, rad,
		  vpd, net_short, air_temp, ra,
		  ppt, f, dt, veg_var->Wdew, elevation,
		  depth, Wcr, Wpwp, &Wdew,
		  &canopyevap, layerevap);

  veg_var->canopyevap = canopyevap;
  veg_var->throughfall = throughfall;
  veg_var->Wdew = Wdew;
  Evap = canopyevap;
  for(i=0;i<options.Nlayer;i++) {
    layer[i].evap = layerevap[i];
    Evap += layerevap[i];
  }
  free((char *)layerevap);

  Evap /= (1000. * dt * 3600.);

  return (Evap);

}

/**********************************************************************
	EVAPOTRANSPIRATION ROUTINE
**********************************************************************/

void transpiration(layer_data_struct *layer,
		   int veg_class, 
		   int month, 
		   double evap_temp,
		   double rad,
		   double vpd,
		   double net_short,
		   double air_temp,
		   double ra,
		   double ppt,
		   double f,
		   double dt,
		   double Wdew,
		   double elevation,
		   double *depth,
		   double *Wcr,
		   double *Wpwp,
		   double *new_Wdew,
		   double *canopyevap,
		   double *layerevap)
/**********************************************************************
  Computes evapotranspiration for unfrozen soils
  Allows for multiple layers.
**********************************************************************/
{
  extern veg_lib_struct *veg_lib;
  extern option_struct options;

  int i;
  double gsm_inv;               	/* soil moisture stress factor */
  double moist1, moist2;                /* tmp holding of moisture */
  double evap;                          /* tmp holding for evap total */
  double Wcr1;                          /* tmp holding of critical water for upper layers */
  double root;                          /* proportion of roots in moist>Wcr zones */
  double spare_evap;                    /* evap for 2nd distribution */
  double avail_moist[MAXlayer];         /* moisture available for trans */

  /********************************************************************** 
     EVAPOTRANSPIRATION

     Calculation of the evapotranspirations
     2.18

     First part: Soil moistures and root fractions of both layers
     influence each other

     Re-written to allow for multi-layers.
  **********************************************************************/
 
  /**************************************************
    Compute moisture content in combined upper layers
    **************************************************/
  moist1 = 0.0;
  Wcr1 = 0.0;                    /* may include this in struct latter */
  for(i=0;i<options.Nlayer-1;i++){
    if(veg_lib[veg_class].root[i] > 0.) {
      if(options.FROZEN_SOIL) 
        avail_moist[i] = layer[i].moist_thaw*(layer[i].tdepth)
	  /depth[i] + layer[i].moist_froz*
	  (layer[i].fdepth-layer[i].tdepth)/depth[i]
	  + layer[i].moist*(depth[i]-layer[i].fdepth)
	  /depth[i];
      else
        avail_moist[i]=layer[i].moist;

      moist1+=avail_moist[i];
      Wcr1 += Wcr[i];
    }
    else avail_moist[i]=0.;
  }

  /*****************************************
    Compute moisture content in lowest layer
    *****************************************/
  i=options.Nlayer-1;
  if(options.FROZEN_SOIL)
    moist2 = layer[i].moist_thaw*(layer[i].tdepth)
      /depth[i] + layer[i].moist_froz*
      (layer[i].fdepth-layer[i].tdepth)/depth[i]
      + layer[i].moist*(depth[i]-layer[i].fdepth)
      /depth[i];
  else
    moist2=layer[i].moist;

  avail_moist[i]=moist2;

  /** Conditions" **/
  /** CASE 1: both layers exceed their Wcr values **/
  /**         OR, 1 layer exceeds Wcr and has 50% plus roots **/
  /** First condition is not mutually exclusive - leave for now **/

  if( (moist1>=Wcr1 && moist2>=Wcr[options.Nlayer-1] && Wcr1>0.) ||
      (moist1>=Wcr1 && (1-veg_lib[veg_class].root[options.Nlayer-1])>= 0.5) ||
      (moist2>=Wcr[options.Nlayer-1] &&
      veg_lib[veg_class].root[options.Nlayer-1]>=0.5) ){
    gsm_inv=1.0;
    evap = penman(rad, vpd * 1000., ra,
           veg_lib[veg_class].rmin,
           veg_lib[veg_class].rarc, veg_lib[veg_class].LAI[month-1], 
           gsm_inv, air_temp, evap_temp,
           net_short, elevation) * dt / 24.0 *
           (1.0-f*pow((Wdew/veg_lib[veg_class].Wdmax[month-1]),
           (2.0/3.0)));

    /** divide up evap based on root distribution **/
    /** Note the indexing of the roots **/
    root=1.0;
    spare_evap=0.0;
    for(i=0;i<options.Nlayer;i++){
      if(avail_moist[i]>=Wcr[i]){
        layerevap[i]=evap*veg_lib[veg_class].root[i];
      }
      else {
          
        if (avail_moist[i] >= Wpwp[i]) 
          gsm_inv = (avail_moist[i] - Wpwp[i]) /
                    (Wcr[i] - Wpwp[i]);
        else 
          gsm_inv=0.0;
	    
        layerevap[i]=evap*gsm_inv*veg_lib[veg_class].root[i];
        root -= veg_lib[veg_class].root[i];
        spare_evap=evap*veg_lib[veg_class].root[i]*(1.0-gsm_inv);
      }
    }

    if(spare_evap>0.0){
      for(i=0;i<options.Nlayer;i++){
        if(avail_moist[i] >= Wcr[i]){
          layerevap[i]+=veg_lib[veg_class].root[i]*spare_evap;
        }
      }
    }
  }

  /****************************************
    CASE 2: Independent evapotranspirations
    ****************************************/

  else {

    for(i=0;i<options.Nlayer;i++){
      if(avail_moist[i] >= Wcr[i])
	gsm_inv=1.0;
      else if(avail_moist[i] >= Wpwp[i])
	gsm_inv=(avail_moist[i] - Wpwp[i]) /
	  (Wcr[i] - Wpwp[i]);
      else 
	gsm_inv=0.0;

      if(gsm_inv > 0.0){
        layerevap[i] = penman(rad, vpd * 1000., ra, 
                 veg_lib[veg_class].rmin,
                 veg_lib[veg_class].rarc, veg_lib[veg_class].LAI[month-1], 
                 gsm_inv, air_temp, evap_temp, 
                 net_short, elevation) 
                 * dt / 24.0 * veg_lib[veg_class].root[i] 
                 * (1.0-f*pow((Wdew/
                 veg_lib[veg_class].Wdmax[month-1]),(2.0/3.0)));
      }
      else layerevap[i] = 0.0;

    }
  }
    
  /** check this -> should this be done relative to losses from upper layer **/
  for(i=0;i<options.Nlayer;i++){
    if(layerevap[i] > layer[i].moist - Wpwp[i]) {
      layerevap[i] = layer[i].moist - Wpwp[i];
    }
    if ( layerevap[i] < 0.0 ) {
/*****
      *new_Wdew -= layerevap[i];
      *canopyevap += layerevap[i];
*****/
      layerevap[i] = 0.0;
    }
  }

}