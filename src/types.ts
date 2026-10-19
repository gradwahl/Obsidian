export type LanguageCode = 'en' | 'pl' | 'es' | 'fr' | 'nl' | 'de' | 'it' | 'zh' | 'ru';

export interface LanguageOption {
  code: LanguageCode;
  name: string;
}

export type SidebarView =
  | 'home'
  | 'games'
  | 'emulators'
  | 'files'
  | 'payloads'
  | 'downloads'
  | 'search'
  | 'settings';

export type PackageFormat = 'exfat' | 'ffpkg' | 'pkg';
export type PackageState = 'available' | 'downloading' | 'installed';

export type HomeCategoryId = 'ps5' | 'ps4' | 'popular' | 'newest' | 'top_rated';

export interface GameItem {
  id_code: string;
  title: string;
  version: string;
  source_name: string;
  is_game: boolean;
  is_dlc: boolean;
  parent_title_id?: string;
  screenshot_urls: string[];
  catalog_added_at?: number; // timestamp
  release_date?: string;
  download_count?: number;
  rating_score?: number;
  description: string;
  description_id?: string;
  install_path: string;
  download_url?: string;
  cover_url?: string;
  cover_local_path?: string;
  boxart_url?: string;
  backdrop_url?: string;
  size_gb: number;
  format: PackageFormat;
  state: PackageState;
  progress: number;
  cover_r: number;
  cover_g: number;
  cover_b: number;
}

export interface LibraryConsole {
  name: string;
  title_id_prefix: string | null;
  artwork_path: string | null;
  logo_path: string | null;
}

export interface LibraryCompany {
  name: string;
  r: number;
  g: number;
  b: number;
  consoles: LibraryConsole[];
  artwork_path: string | null;
}

export interface CatalogFilters {
  sources_limited: boolean;
  sources: string[];
  disabled_formats: PackageFormat[];
}

export interface SystemStats {
  data_free_gb: number;
  data_total_gb: number;
  ext0_free_gb: number;
  ext0_total_gb: number;
  cpu_temp_c: number;
  gpu_temp_c: number;
  network_online: boolean;
  ip_address: string;
}
