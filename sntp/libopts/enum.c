
/**
 * \file enumeration.c
 *
 *  Handle options with enumeration names and bit mask bit names
 *  for their arguments.
 *
 * @addtogroup autoopts
 * @{
 */
/*
 *  This routine will run run-on options through a pager so the
 *  user may examine, print or edit them at their leisure.
 *
 *  This file is part of AutoOpts, a companion to AutoGen.
 *  AutoOpts is free software.
 *  AutoOpts is Copyright (C) 1992-2013 by Bruce Korb - all rights reserved
 *
 *  AutoOpts is available under any one of two licenses.  The license
 *  in use must be one of these two and the choice is under the control
 *  of the user of the license.
 *
 *   The GNU Lesser General Public License, version 3 or later
 *      See the files "COPYING.lgplv3" and "COPYING.gplv3"
 *
 *   The Modified Berkeley Software Distribution License
 *      See the file "COPYING.mbsd"
 *
 *  These files have the following sha256 sums:
 *
 *  8584710e9b04216a394078dc156b781d0b47e1729104d666658aecef8ee32e95  COPYING.gplv3
 *  4379e7444a0e2ce2b12dd6f5a52a27a4d02d39d247901d3285c88cf0d37f477b  COPYING.lgplv3
 *  13aa749a5b0a454917a944ed8fffc530b784f5ead522b1aacaf4ec8aa55a6239  COPYING.mbsd
 */

static char const * pz_enum_err_fmt;

/* = = = START-STATIC-FORWARD = = = */
static void
enum_err(tOptions * pOpts, tOptDesc * pOD,
         char const * const * paz_names, int name_ct);

static uintptr_t
find_name(char const * name, tOptions * pOpts, tOptDesc * pOD,
          char const * const *  paz_names, unsigned int name_ct);

static void
set_memb_shell(tOptions * pOpts, tOptDesc * pOD, char const * const * paz_names,
               unsigned int name_ct);

static void
set_memb_names(tOptions * pOpts, tOptDesc * pOD, char const * const * paz_names,
               unsigned int name_ct);
/* = = = END-STATIC-FORWARD = = = */

static void
enum_err(tOptions * pOpts, tOptDesc * pOD,
         char const * const * paz_names, int name_ct)
{
    size_t max_len = 0;
    size_t ttl_len = 0;
    int    ct_down = name_ct;
    int    hidden  = 0;

    /*
     *  A real "pOpts" pointer means someone messed up.  Give a real error.
     */
    if (pOpts > OPTPROC_EMIT_LIMIT)
        fprintf(option_usage_fp, pz_enum_err_fmt, pOpts->pzProgName,
                pOD->optArg.argString, pOD->pz_Name);

    fprintf(option_usage_fp, zValidKeys, pOD->pz_Name);

    /*
     *  If the first name starts with this funny character, then we have
     *  a first value with an unspellable name.  You cannot specify it.
     *  So, we don't list it either.
     */
    if (**paz_names == 0x7F) {
        paz_names++;
        hidden  = 1;
        ct_down = --name_ct;
    }

    /*
     *  Figure out the maximum length of any name, plus the total length
     *  of all the names.
     */
    {
        char const * const * paz = paz_names;

        do  {
            size_t len = strlen(*(paz++)) + 1;
            if (len > max_len)
                max_len = len;
            ttl_len += len;
        } while (--ct_down > 0);

        ct_down = name_ct;
    }

    /*
     *  IF any one entry is about 1/2 line or longer, print one per line
     */
    if (max_len > 35) {
        do  {
            fprintf(option_usage_fp, ENUM_ERR_LINE, *(paz_names++));
        } while (--ct_down > 0);
    }

    /*
     *  ELSE IF they all fit on one line, then do so.
     */
    else if (ttl_len < 76) {
        fputc(' ', option_usage_fp);
        do  {
            fputc(' ', option_usage_fp);
            fputs(*(paz_names++), option_usage_fp);
        } while (--ct_down > 0);
        fputc(NL, option_usage_fp);
    }

    /*
     *  Otherwise, columnize the output
     */
    else {
        unsigned int ent_no = 0;
        char  zFmt[16];  /* format for all-but-last entries on a line */

        sprintf(zFmt, ENUM_ERR_WIDTH, (int)max_len);
        max_len = 78 / max_len; /* max_len is now max entries on a line */
        fputs(TWO_SPACES_STR, option_usage_fp);

        /*
         *  Loop through all but the last entry
         */
        ct_down = name_ct;
        while (--ct_down > 0) {
            if (++ent_no == max_len) {
                /*
                 *  Last entry on a line.  Start next line, too.
                 */
                fprintf(option_usage_fp, NLSTR_SPACE_FMT, *(paz_names++));
                ent_no = 0;
            }

            else
                fprintf(option_usage_fp, zFmt, *(paz_names++) );
        }
        fprintf(option_usage_fp, NLSTR_FMT, *paz_names);
    }

    if (pOpts > OPTPROC_EMIT_LIMIT) {
        fprintf(option_usage_fp, zIntRange, hidden, name_ct - 1 + hidden);

        (*(pOpts->pUsageProc))(pOpts, EXIT_FAILURE);
        /* NOTREACHED */
    }

    if (OPTST_GET_ARGTYPE(pOD->fOptState) == OPARG_TYPE_MEMBERSHIP) {
        fprintf(option_usage_fp, zLowerBits, name_ct);
        fputs(zSetMemberSettings, option_usage_fp);
    } else {
        fprintf(option_usage_fp, zIntRange, hidden, name_ct - 1 + hidden);
    }
}

/**
 * Convert a name or number into a binary number.
 * "~0" and "-1" will be converted to the largest value in the enumeration.
 *
 * @param name       the keyword name (number) to convert
 * @param pOpts      the program's option descriptor
 * @param pOD        the option descriptor for this option
 * @param paz_names  the list of keywords for this option
 * @param name_ct    the count of keywords
 */
static uintptr_t
find_name(char const * name, tOptions * pOpts, tOptDesc * pOD,
          char const * const *  paz_names, unsigned int name_ct)
{
    /*
     *  Return the matching index as a pointer sized integer.
     *  The result gets stashed in a char* pointer.
     */
    uintptr_t   res = name_ct;
    size_t      len = strlen((char*)name);
    uintptr_t   idx;

    if (IS_DEC_DIGIT_CHAR(*name)) {
        char * pz = (char *)(void *)name;
        unsigned long val = strtoul(pz, &pz, 0);
        if ((*pz == NUL) && (val < name_ct))
            return (uintptr_t)val;
        pz_enum_err_fmt = znum_too_large;
        option_usage_fp = stderr;
        enum_err(pOpts, pOD, paz_names, (int)name_ct);
        return name_ct;
    }

    if (IS_INVERSION_CHAR(*name) && (name[2] == NUL)) {
        if (  ((name[0] == '~') && (name[1] == '0'))
           || ((name[0] == '-') && (name[1] == '1')))
        return (uintptr_t)(name_ct - 1);
        goto oops;
    }

    /*
     *  Look for an exact match, but remember any partial matches.
     *  Multiple partial matches means we have an ambiguous match.
     */
    for (idx = 0; idx < name_ct; idx++) {
        if (strncmp((char*)paz_names[idx], (char*)name, len) == 0) {
            if (paz_names[idx][len] == NUL)
                return idx;  /* full match */

            if (res == name_ct)
                res = idx; /* save partial match */
            else
                res = (uintptr_t)~0;  /* may yet find full match */
        }
    }

    if (res < name_ct)
        return res; /* partial match */

 oops:

    pz_enum_err_fmt = (res == name_ct) ? zNoKey : zambiguous_key;
    option_usage_fp = stderr;
    enum_err(pOpts, pOD, paz_names, (int)name_ct);
    return name_ct;
}


/*=export_func  optionKeywordName
 * what:  Convert between enumeration values and strings
 * private:
 *
 * arg:   tOptDesc*,     pOD,       enumeration option description
 * arg:   unsigned int,  enum_val,  the enumeration value to map
 *
 * ret_type:  char const *
 * ret_desc:  the enumeration name from const memory
 *
 * doc:   This converts an enumeration value into the matching string.
=*/
char const *
optionKeywordName(tOptDesc * pOD, unsigned int enum_val)
{
    tOptDesc od = { 0 };
    od.optArg.argEnum = enum_val;

    (*(pOD->pOptProc))(OPTPROC_RETURN_VALNAME, &od );
    return od.optArg.argString;
}


/*=export_func  optionEnumerationVal
 * what:  Convert from a string to an enumeration value
 * private:
 *
 * arg:   tOptions*,     pOpts,     the program options descriptor
 * arg:   tOptDesc*,     pOD,       enumeration option description
 * arg:   char const * const *,  paz_names, list of enumeration names
 * arg:   unsigned int,  name_ct,   number of names in list
 *
 * ret_type:  uintptr_t
 * ret_desc:  the enumeration value
 *
 * doc:   This converts the optArg.argString string from the option description
 *        into the index corresponding to an entry in the name list.
 *        This will match the generated enumeration value.
 *        Full matches are always accepted.  Partial matches are accepted
 *        if there is only one partial match.
=*/
uintptr_t
optionEnumerationVal(tOptions * pOpts, tOptDesc * pOD,
                     char const * const * paz_names, unsigned int name_ct)
{
    uintptr_t res = 0UL;

    /*
     *  IF the program option descriptor pointer is invalid,
     *  then it is some sort of special request.
     */
    switch ((uintptr_t)pOpts) {
    case (uintptr_t)OPTPROC_EMIT_USAGE:
        /*
         *  print the list of enumeration names.
         */
        enum_err(pOpts, pOD, paz_names, (int)name_ct);
        break;

    case (uintptr_t)OPTPROC_EMIT_SHELL:
    {
        unsigned int ix = (unsigned int)pOD->optArg.argEnum;
        /*
         *  print the name string.
         */
        if (ix >= name_ct)
            printf(INVALID_FMT, ix);
        else
            fputs(paz_names[ ix ], stdout);

        break;
    }

    case (uintptr_t)OPTPROC_RETURN_VALNAME:
    {
        unsigned int ix = (unsigned int)pOD->optArg.argEnum;
        /*
         *  Replace the enumeration value with the name string.
         */
        if (ix >= name_ct)
            return (uintptr_t)INVALID_STR;

        pOD->optArg.argString = paz_names[ix];
        break;
    }

    default:
        if ((pOD->fOptState & OPTST_RESET) != 0)
            break;

        res = find_name(pOD->optArg.argString, pOpts, pOD, paz_names, name_ct);

        if (pOD->fOptState & OPTST_ALLOC_ARG) {
            AGFREE(pOD->optArg.argString);
            pOD->fOptState &= ~OPTST_ALLOC_ARG;
            pOD->optArg.argString = NULL;
        }
    }

    return res;
}

static void
set_memb_shell(tOptions * pOpts, tOptDesc * pOD, char const * const * paz_names,
               unsigned int name_ct)
{
    /*
     *  print the name string.
     */
    unsigned int ix =  0;
    uintptr_t  bits = (uintptr_t)pOD->optCookie;
    size_t     len  = 0;

    (void)pOpts;
    bits &= ((uintptr_t)1 << (uintptr_t)name_ct) - (uintptr_t)1;

    while (bits != 0) {
        if (bits & 1) {
            if (len++ > 0) fputs(OR_STR, stdout);
            fputs(paz_names[ix], stdout);
        }
        if (++ix >= name_ct) break;
        bits >>= 1;
    }
}

static void
set_memb_names(tOptions * pOpts, tOptDesc * pOD, char const * const * paz_names,
               unsigned int name_ct)
{
    char *     pz;
    uintptr_t  bits = (uintptr_t)pOD->optCookie;
    unsigned int ix = 0;
    size_t     len  = NONE_STR_LEN + 1;

    (void)pOpts;
    bits &= ((uintptr_t)1 << (uintptr_t)name_ct) - (uintptr_t)1;

    /*
     *  Replace the enumeration value with the name string.
     *  First, determine the needed length, then allocate and fill in.
     */
    while (bits != 0) {
        if (bits & 1)
            len += strlen(paz_names[ix]) + PLUS_STR_LEN + 1;
        if (++ix >= name_ct) break;
        bits >>= 1;
    }

    pOD->optArg.argString = pz = AGALOC(len, "enum");

    /*
     *  Start by clearing all the bits.  We want to turn off any defaults
     *  because we will be restoring to current state, not adding to
     *  the default set of bits.
     */
    memcpy(pz, NONE_STR, NONE_STR_LEN);
    pz += NONE_STR_LEN;
    bits = (uintptr_t)pOD->optCookie;
    bits &= ((uintptr_t)1 << (uintptr_t)name_ct) - (uintptr_t)1;
    ix = 0;

    while (bits != 0) {
        if (bits & 1) {
            size_t nln = strlen(paz_names[ix]);
            memcpy(pz, PLUS_STR, PLUS_STR_LEN);
            memcpy(pz+PLUS_STR_LEN, paz_names[ix], nln);
            pz += nln + PLUS_STR_LEN;
        }
        if (++ix >= name_ct) break;
        bits >>= 1;
    }
    *pz = NUL;
}

/*=export_func  optionSetMembers
 * what:  Convert between bit flag values and strings
 * private:
 *
 * arg:   tOptions*,     pOpts,     the program options descriptor
 * arg:   tOptDesc*,     pOD,       enumeration option description
 * arg:   char const * const *,
 *                       paz_names, list of enumeration names
 * arg:   unsigned int,  name_ct,   number of names in list
 *
 * doc:   This converts the optArg.argString string from the option description
 *        into the index corresponding to an entry in the name list.
 *        This will match the generated enumeration value.
 *        Full matches are always accepted.  Partial matches are accepted
 *        if there is only one partial match.
=*/
void
optionSetMembers(tOptions * pOpts, tOptDesc * pOD,
                 char const* const * paz_names, unsigned int name_ct)
{
    /*
     *  IF the program option descriptor pointer is invalid,
     *  then it is some sort of special request.
     */
    switch ((uintptr_t)pOpts) {
    case (uintptr_t)OPTPROC_EMIT_USAGE:
        enum_err(OPTPROC_EMIT_USAGE, pOD, paz_names, name_ct);
        return;

    case (uintptr_t)OPTPROC_EMIT_SHELL:
        set_memb_shell(pOpts, pOD, paz_names, name_ct);
        return;

    case (uintptr_t)OPTPROC_RETURN_VALNAME:
        set_memb_names(pOpts, pOD, paz_names, name_ct);
        return;

    default:
        break;
    }

    if ((pOD->fOptState & OPTST_RESET) != 0)
        return;

    {
        char const * pzArg = pOD->optArg.argString;
        uintptr_t res;
        if ((pzArg == NULL) || (*pzArg == NUL)) {
            pOD->optCookie = (void*)0;
            return;
        }

        res = (uintptr_t)pOD->optCookie;
        for (;;) {
            int  iv, len;

            pzArg = SPN_SET_SEPARATOR_CHARS(pzArg);
            iv = (*pzArg == '!');
            if (iv)
                pzArg = SPN_WHITESPACE_CHARS(pzArg+1);

            len = (int)(BRK_SET_SEPARATOR_CHARS(pzArg) - pzArg);
            if (len == 0)
                break;

            if ((len == 3) && (strncmp(pzArg, zAll, 3) == 0)) {
                if (iv)
                     res = 0;
                else res = ~0UL;
            }
            else if ((len == 4) && (strncmp(pzArg, zNone, 4) == 0)) {
                if (! iv)
                    res = 0;
            }
            else do {
                char* pz;
                uintptr_t bit = strtoul(pzArg, &pz, 0);

                if (pz != pzArg + len) {
                    char z[ AO_NAME_SIZE ];
                    char const* p;
                    unsigned int shift_ct;

                    if (*pz != NUL) {
                        if (len >= AO_NAME_LIMIT)
                            break;
                        memcpy(z, pzArg, (size_t)len);
                        z[len] = NUL;
                        p = z;
                    } else {
                        p = pzArg;
                    }

                    shift_ct = (unsigned int)
                        find_name(p, pOpts, pOD, paz_names, name_ct);
                    if (shift_ct >= name_ct) {
                        pOD->optCookie = (void*)0;
                        return;
                    }
                    bit = 1UL << shift_ct;
                }
                if (iv)
                     res &= ~bit;
                else res |= bit;
            } while (false);

            if (pzArg[len] == NUL)
                break;
            pzArg += len + 1;
        }
        if (name_ct < (8 * sizeof(uintptr_t))) {
            res &= (1UL << name_ct) - 1UL;
        }

        pOD->optCookie = (void*)res;
    }
}

/** @}
 *
 * Local Variables:
 * mode: C
 * c-file-style: "stroustrup"
 * indent-tabs-mode: nil
 * End:
 * end of autoopts/enum.c */
