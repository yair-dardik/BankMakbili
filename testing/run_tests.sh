\
    #!/usr/bin/env bash
    set -euo pipefail

    ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    PROJ_ROOT="$(cd "$ROOT/.." && pwd)"
    CASES_DIR="$ROOT/cases"
    OUT_DIR="$ROOT/out"

    RED='\033[0;31m'; GRN='\033[0;32m'; YLW='\033[1;33m'; NC='\033[0m'
    say()  { echo -e "${GRN}[INFO]${NC} $*"; }
    warn() { echo -e "${YLW}[WARN]${NC} $*"; }
    die()  { echo -e "${RED}[FAIL]${NC} $*"; exit 1; }

    need() { command -v "$1" >/dev/null 2>&1 || die "Missing command: $1"; }
    need make; need timeout; need grep; need awk; need sed; need date

    if [[ ! -f "$PROJ_ROOT/Makefile" ]]; then
      die "Makefile not found in project root: $PROJ_ROOT"
    fi

    # --- build ---
    say "Building project (make clean && make) ..."
    (cd "$PROJ_ROOT" && make clean >/dev/null 2>&1 || true)
    (cd "$PROJ_ROOT" && make) | tee "$OUT_DIR/build_$(date +%Y%m%d_%H%M%S).txt"

    if [[ ! -x "$PROJ_ROOT/bank" ]]; then
      die "Executable '$PROJ_ROOT/bank' not found after build."
    fi

    # Detect 'main stub' quickly (common pitfall)
    if [[ -f "$PROJ_ROOT/main.cpp" ]]; then
      if grep -q "Minimal stub" "$PROJ_ROOT/main.cpp"; then
        warn "main.cpp נראה כמו stub (לא מפעיל Bank/ATM). רוב הטסטים ייכשלו עד שתממש main אמיתי."
      fi
    fi

    pass=0
    fail=0

    run_case() {
      local case_name="$1"
      local case_path="$CASES_DIR/$case_name"
      local vip_threads timeout_s
      vip_threads="$(awk -F= '/^vip_threads=/{print $2}' "$case_path/spec.txt" 2>/dev/null || true)"
      timeout_s="$(awk -F= '/^timeout=/{print $2}' "$case_path/spec.txt" 2>/dev/null || true)"

      # fallback if spec.txt missing (should not happen)
      [[ -n "${vip_threads:-}" ]] || vip_threads="1"
      [[ -n "${timeout_s:-}" ]] || timeout_s="6"

      local stamp; stamp="$(date +%Y%m%d_%H%M%S)"
      local run_dir="$OUT_DIR/${case_name}_$stamp"
      mkdir -p "$run_dir"

      # Reset log for determinism
      rm -f "$PROJ_ROOT/log.txt"

      local atm_files=()
      while IFS= read -r -d '' f; do atm_files+=("$f"); done < <(find "$case_path" -maxdepth 1 -name "atm*.txt" -print0 | sort -z)

      if [[ ${#atm_files[@]} -eq 0 ]]; then
        die "No atm*.txt files in $case_path"
      fi

      say "==> Running case: $case_name (VIP threads=$vip_threads, timeout=${timeout_s}s)"
      set +e
      (cd "$PROJ_ROOT" && timeout "${timeout_s}s" ./bank "$vip_threads" "${atm_files[@]}") \
          >"$run_dir/stdout.txt" 2>"$run_dir/stderr.txt"
      rc=$?
      set -e

      cp -f "$PROJ_ROOT/log.txt" "$run_dir/log.txt" 2>/dev/null || true

      # basic check: no "illegal arguments"
      if grep -q "illegal arguments" "$run_dir/stderr.txt"; then
        echo -e "${RED}  ✗ stderr contains 'illegal arguments'${NC}"
        return 1
      fi

      # run per-case checks
      local ok=1
      while IFS= read -r checkline; do
        [[ -z "$checkline" ]] && continue
        local kind rest
        kind="${checkline%%|*}"
        rest="${checkline#*|}"

        case "$kind" in
          log)
            local pat desc
            pat="${rest%%|*}"; desc="${rest#*|}"
            if [[ -f "$run_dir/log.txt" ]] && grep -qE "$pat" "$run_dir/log.txt"; then
              echo -e "  ${GRN}✓${NC} $desc"
            else
              echo -e "  ${RED}✗${NC} $desc"
              ok=0
            fi
            ;;
          log_absent)
            local pat desc
            pat="${rest%%|*}"; desc="${rest#*|}"
            if [[ ! -f "$run_dir/log.txt" ]] || ! grep -qE "$pat" "$run_dir/log.txt"; then
              echo -e "  ${GRN}✓${NC} $desc"
            else
              echo -e "  ${RED}✗${NC} $desc"
              ok=0
            fi
            ;;
          stdout_count)
            local pat want desc
            pat="${rest%%|*}"; rest2="${rest#*|}"
            want="${rest2%%|*}"; desc="${rest2#*|}"
            local got
            got="$(grep -cE "$pat" "$run_dir/stdout.txt" || true)"
            if [[ "$got" -ge "$want" ]]; then
              echo -e "  ${GRN}✓${NC} $desc (found=$got)"
            else
              echo -e "  ${RED}✗${NC} $desc (found=$got, want>=$want)"
              ok=0
            fi
            ;;
          log_order)
            # expects two patterns: first must appear before second
            local pat1 pat2 desc
            pat1="${rest%%|*}"; rest2="${rest#*|}"
            pat2="${rest2%%|*}"; desc="${rest2#*|}"
            if [[ ! -f "$run_dir/log.txt" ]]; then
              echo -e "  ${RED}✗${NC} $desc (no log.txt)"
              ok=0
            else
              local l1 l2
              l1="$(grep -nE "$pat1" "$run_dir/log.txt" | head -n1 | cut -d: -f1 || true)"
              l2="$(grep -nE "$pat2" "$run_dir/log.txt" | head -n1 | cut -d: -f1 || true)"
              if [[ -n "$l1" && -n "$l2" && "$l1" -lt "$l2" ]]; then
                echo -e "  ${GRN}✓${NC} $desc (line $l1 < $l2)"
              else
                echo -e "  ${RED}✗${NC} $desc (line1=$l1, line2=$l2)"
                ok=0
              fi
            fi
            ;;
          balance_after_rollback)
            # parse last "balance is X ILS" line for account 1
            local expect desc
            expect="${rest%%|*}"; desc="${rest#*|}"
            if [[ ! -f "$run_dir/log.txt" ]]; then
              echo -e "  ${RED}✗${NC} $desc (no log.txt)"
              ok=0
            else
              local line ils
              line="$(grep -E "Account 1 balance is [0-9]+ ILS" "$run_dir/log.txt" | tail -n1 || true)"
              ils="$(echo "$line" | sed -nE 's/.*balance is ([0-9]+) ILS.*/\1/p')"
              if [[ -n "$ils" && "$ils" -eq "$expect" ]]; then
                echo -e "  ${GRN}✓${NC} $desc (got=$ils)"
              else
                echo -e "  ${RED}✗${NC} $desc (got=${ils:-N/A}, expect=$expect)"
                ok=0
              fi
            fi
            ;;
          *)
            warn "Unknown check kind: $kind"
            ;;
        esac
      done < "$case_path/checks.txt"

      # Save exit code info
      echo "$rc" > "$run_dir/exit_code.txt"

      if [[ $rc -eq 124 ]]; then
        warn "  (timeout) program didn't exit within ${timeout_s}s for $case_name"
      elif [[ $rc -ne 0 ]]; then
        warn "  program exited with code $rc for $case_name"
      fi

      if [[ $ok -eq 1 ]]; then
        return 0
      else
        return 1
      fi
    }

    # iterate cases in order
    for case_path in "$CASES_DIR"/*; do
      [[ -d "$case_path" ]] || continue
      case_name="$(basename "$case_path")"
      if run_case "$case_name"; then
        pass=$((pass+1))
      else
        fail=$((fail+1))
      fi
      echo
    done

    say "Done. PASS=$pass FAIL=$fail"
    if [[ $fail -ne 0 ]]; then
      exit 1
    fi
    exit 0
