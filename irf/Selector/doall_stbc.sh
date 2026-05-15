#! /bin/bash
set -u

proc_ver="v9.0"
output_dir="/project/antares/fvazquez/irf/rootfiles/Selector"
cut="muonscore<0.1 && noisescore < 0.5 && runs_neutrino2024_veto_sparks && (Evt->trks[0].dir.z>0 || Evt->trks[1].dir.z>0)"
data_dir="/data/antares/users/hlumengo/Merged_dsts"
pid_dir="/data/antares/users/hlumengo/PID/${proc_ver}_selection_trees"
treename="sel"

CallSelector () {
  local detid=$1
  local quality=$2
  local run=$3
  local type=$4
  local snakemake_fileext=$5
  outputfile="$output_dir/KM3NeT_00000${detid}_${quality}_${run:0:8}_${type}_${proc_ver}.root" #{run:0-8} to address typos there...
  inputname="KM3NeT_00000${detid}_${quality}_${run}.${snakemake_fileext}.${proc_ver}.root"
  inputfile="$data_dir/KM3NeT_00000${detid}_${quality}/${inputname}"
  pidfile="$pid_dir/KM3NeT_00000${detid}_${quality}/SelectedEventsTree_PID_${inputname}.root"
  $IRF_DIR/Selector/SelectorFromJSON -o "$outputfile" -c "$cut" -i "$inputfile" -f "$pidfile,$treename"
}


for detid in "049" "100" "132"
do
  for quality in "Qlow1" "bestQ"
  do

    # data
    snakemake_fileext="data.jterbr.jppmuon_jppshower-upgoing_dynamic.offline.dst"
    declare -a runlist_data=($(ls -h $data_dir/KM3NeT_00000${detid}_${quality} | grep -oE '_[0-9|X]{8,}\.data' | grep -oE '[0-9|X]{8,}'))
    for run in "${runlist_data[@]}"
    do
      CallSelector "$detid" "$quality" "$run" "data" "$snakemake_fileext"
    done

    # muons
    snakemake_fileext="mc.mupage.jterbr.jppmuon_jppshower-upgoing_static.offline.dst"
    declare -a runlist_muons=($(ls -h $data_dir/KM3NeT_00000${detid}_${quality} | grep -oE '_[0-9|X]{8,}\.mc\.mupage' | grep -oE '[0-9|X]{8,}'))
    for run in "${runlist_muons[@]}"
    do
      CallSelector "$detid" "$quality" "$run" "mu" "$snakemake_fileext"
    done

    # neutrinos
    snakemake_fileext="mc.gsg_neutrinos.jterbr.jppmuon_jppshower-upgoing_static.offline.dst"
    run="000XXXXX"
    CallSelector "$detid" "$quality" "$run" "nu" "$snakemake_fileext"

    # noise
    snakemake_fileext="mc.pure_noise.jterbr.jppmuon_jppshower-upgoing_static.offline.dst"
    run="000XXXXX"
    CallSelector "$detid" "$quality" "$run" "noise" "$snakemake_fileext"

  done
done


