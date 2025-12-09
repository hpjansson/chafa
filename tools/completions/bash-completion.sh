_chafa()
{
  local cur prev opts
  COMPREPLY=()
  cur="${COMP_WORDS[COMP_CWORD]}"
  prev="${COMP_WORDS[COMP_CWORD-1]}"

  opts="--help -h --version -v --verbose --probe --files --files0 --format -f --optimize -O --relative --passthrough --polite --align --clear --center -C --exact-size --fit-width --font-ratio --grid -g --label -l --link --margin-bottom --margin-right --scale --size -s --stretch --view-size --animate --duration -d --speed --watch --bg --colors -c --color-extractor --color-space --dither --dither-grain --dither-intensity --fg --invert --preprocess -p --threshold -t --threads --work -w --fg-only --fill --glyph-file --symbols --dump-detect --fuzz-options --zoom"

  if [[ ${cur} == -* ]] ; then
    COMPREPLY=( $(compgen -W "${opts}" -- "${cur}") )
    return 0
  fi

  case "${prev}" in
    --format|-f)
      COMPREPLY=( $(compgen -W "iterm kitty sixels symbols" -- "${cur}") )
      ;;
    --optimize|-O)
      COMPREPLY=( $(compgen -W "0 1 2 3 4 5 6 7 8 9" -- "${cur}") )
      ;;
    --relative|--polite|--animate|--preprocess|-p|--center|-C|--label)
      COMPREPLY=( $(compgen -W "on off" -- "${cur}") )
      ;;
    --passthrough)
      COMPREPLY=( $(compgen -W "auto none screen tmux" -- "${cur}") )
      ;;

    --exact-size|--link|--probe)
      COMPREPLY=( $(compgen -W "auto on off" -- "${cur}") )
      ;;
    --grid)
      COMPREPLY=( $(compgen -W "auto" -- "${cur}") )
      ;;
    --scale)
      COMPREPLY=( $(compgen -W "max" -- "${cur}") )
      ;;

    --colors|-c)
      COMPREPLY=( $(compgen -W "none 2 8 16/8 16 240 256 full" -- "${cur}") )
      ;;
    --color-extractor)
      COMPREPLY=( $(compgen -W "average median" -- "${cur}") )
      ;;
    --color-space)
      COMPREPLY=( $(compgen -W "rgb din99d" -- "${cur}") )
      ;;
    --dither)
      COMPREPLY=( $(compgen -W "none ordered diffusion noise" -- "${cur}") )
      ;;

    --fill|--symbols)
      local symbols="all asci braille extra imported narrow solid ugly alnum bad diagonal geometric inverted none space vhalf alpha block digit half latin quad stipple wedge ambiguous border dot hhalf legacy sextant technical wide"
      COMPREPLY=( $(compgen -W "${symbols}" -- "${cur}") )
      ;;

    --work|-w)
      COMPREPLY=( $(compgen -W "1 2 3 4 5 6 7 8 9" -- "${cur}") )
      ;;
  esac

  return 0
}

complete -o default -F _chafa chafa
