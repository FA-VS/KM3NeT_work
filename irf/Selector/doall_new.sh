#! /bin/bash
set -u

# THIS IS JUST AN EXAMPLE SCRIPT!!
# Make sure to update the file paths, name formats (in CallSelector) and snakemake_fileext's before trying to run it!

proc_ver="v9.0"
cut="muonscore<0.1 && noisescore < 0.5 && runs_neutrino2024_veto_sparks && (Evt->trks[0].dir.z>0 || Evt->trks[1].dir.z>0)"

output_dir="/project/antares/fvazquez/irf/rootfiles/Selector"
data_dir="/data/antares/users/hlumengo/Merged_dsts"
pid_dir="/data/antares/users/hlumengo/PID/${proc_ver}_selection_trees"
pidtreename="sel"

CallSelector () {
  local detid=$1
  local run=$2
  local type=$3
  local snakemake_fileext=$4

  outputfile="$output_dir/KM3NeT_${detid}_${run}_${type}_${proc_ver}.root"
  inputname="KM3NeT_${detid}_${run}.${snakemake_fileext}.${proc_ver}.root"
  inputfile="$data_dir/KM3NeT_${detid}/${inputname}"
  pidfile="$pid_dir/KM3NeT_${detid}/SelectedEventsTree_PID_${inputname}.root"

  $IRF_DIR/Selector/SelectorFromJSON -o "$outputfile" -c "$cut" -i "$inputfile" -f "$pidfile,$pidtreename"
}


for detid in "00000049" "00000132"
do

  # muons
  snakemake_fileext="mc.mupage.jterbr.jppmuon_jppshower-upgoing_static.offline.dst"
  declare -a runlist_muons=($(ls -h "$data_dir/KM3NeT_${detid}" | grep -oE '_[0-9|X]{8,}\.mc\.mupage' | grep -oE '[0-9|X]{8,}'))
  for run in "${runlist_muons[@]}"
  do
    CallSelector "$detid" "$run" "mu" "$snakemake_fileext"
  done

  # neutrinos
  snakemake_fileext="mc.gsg_neutrinos.jterbr.jppmuon_jppshower-upgoing_static.offline.dst"
  run="000XXXXX"
  CallSelector "$detid" "$run" "nu" "$snakemake_fileext"

done


