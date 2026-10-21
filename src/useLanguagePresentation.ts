import { useEffect, useState } from 'react';
import { LanguageCode } from './types';

// Presentation only: translation loading and persisted selection remain in App.
export function useLanguagePresentation(language: LanguageCode) {
  const [displayLanguage, setDisplayLanguage] = useState(language);
  const [fading, setFading] = useState(false);
  useEffect(() => {
    if (language === displayLanguage) return;
    setFading(true);
    const timer = window.setTimeout(() => {
      setDisplayLanguage(language);
      setFading(false);
    }, 90);
    return () => window.clearTimeout(timer);
  }, [language, displayLanguage]);
  return { displayLanguage, fading };
}
