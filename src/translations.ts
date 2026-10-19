import { LanguageCode, LanguageOption } from './types';
import translationsData from './translations.json';

export const LANGUAGES: LanguageOption[] = [
  { code: 'pl', name: 'Polski' },
  { code: 'en', name: 'English' },
  { code: 'fr', name: 'Français' },
  { code: 'nl', name: 'Nederlands' },
  { code: 'de', name: 'Deutsch' },
  { code: 'es', name: 'Español' },
  { code: 'it', name: 'Italiano' },
  { code: 'zh', name: '简体中文' },
  { code: 'ru', name: 'Русский' }
];

export function t(
  lang: LanguageCode,
  key: string,
  ...args: (string | number)[]
): string {
  const table = translationsData as Record<string, Record<LanguageCode, string>>;
  const entry = table[key];
  if (!entry) return key;

  let str = entry[lang] || entry['en'] || key;

  // Replace printf-style format specifiers (%s, %d, %.1f, %.0f%%)
  if (args.length > 0) {
    let argIdx = 0;
    // Replace %s, %d, %.1f, %.0f, etc.
    str = str.replace(/%(\.?\d*)?[sdf%]/g, (match) => {
      if (match === '%%') return '%';
      if (argIdx >= args.length) return match;
      const val = args[argIdx++];
      if (match.includes('f') && typeof val === 'number') {
        const decimalsMatch = match.match(/\.(\d+)f/);
        const decimals = decimalsMatch ? parseInt(decimalsMatch[1], 10) : 1;
        return val.toFixed(decimals);
      }
      return String(val);
    });
  }

  return str;
}
