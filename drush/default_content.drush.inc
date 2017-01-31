<?php

/**
 * @file
 * Drush integration for the default_content module.
 */

use Drush\Log\LogLevel;
use Drupal\Core\StringTranslation\PluralTranslatableMarkup;

/**
 * Implements hook_drush_command().
 */
function default_content_drush_command() {
  $items['default-content-export'] = [
    'description' => dt('Exports a single entity'),
    'arguments' => [
      'entity_type' => dt('The entity type to export.'),
      'entity_id' => dt('The ID of the entity to export.'),
    ],
    'options' => [
      'file' => dt('Write out the exported content to a file instead of stdout'),
    ],
    'aliases' => ['dce'],
    'required-arguments' => 2,
  ];
  $items['default-content-export-references'] = [
    'description' => dt('Exports an entity and all its referenced entities.'),
    'arguments' => [
      'entity_type' => dt('The entity type to export.'),
      'entity_id' => dt('The ID of the entity to export.'),
    ],
    'options' => [
      'folder' => dt('Folder to export to, entities are grouped by entity type into directories.'),
    ],
    'aliases' => ['dcer'],
    'required-arguments' => 1,
  ];
  $items['default-content-export-module'] = [
    'description' => dt('Exports all the content defined in a module info file.'),
    'arguments' => [
      'module' => dt('The name of the module.'),
    ],
    'aliases' => ['dcem'],
    'required-arguments' => 1,
  ];
  $items['default-content-export-modules'] = [
    'description' => dt('Exports all the content defined in all enabled modules.'),
    'aliases' => ['dcea'],
  ];
  $items['default-content-import-module'] = [
    'description' => dt('Imports all the content defined in a module info file.'),
    'arguments' => [
      'module' => dt('The name of the module or profile to import.'),
    ],
    'options' => [
      'update' => dt('Update existing entities.'),
    ],
    'aliases' => ['dcim'],
    'required-arguments' => 1,
  ];
  $items['default-content-import-modules'] = [
    'description' => dt('Imports the content provided by installed modules and default profile.'),
    'options' => [
      'update' => dt('Update existing entities.'),
    ],
    'aliases' => ['dcia'],
  ];

  return $items;
}

/**
 * Exports a piece of content into the stdout or into a file.
 *
 * @param string $entity_type_id
 *   The entity type ID.
 * @param mixed $entity_id
 *   The entity ID to export.
 */
function drush_default_content_export($entity_type_id, $entity_id) {
  /** @var \Drupal\default_content\DefaultContentManagerInterface $manager */
  $manager = \Drupal::service('default_content.manager');
  $export = $manager->exportContent($entity_type_id, $entity_id);

  if ($file = drush_get_option('file')) {
    file_put_contents($file, $export);
  }
  else {
    drush_print($export);
  }
}

/**
 * Exports a piece of content and all its referenced entities.
 *
 * @param string $entity_type_id
 *   The entity type ID.
 * @param mixed $entity_id
 *   (Optional) The entity ID to export or all entities will be exported.
 */
function drush_default_content_export_references($entity_type_id, $entity_id = NULL) {
  /** @var \Drupal\default_content\DefaultContentManagerInterface $manager */
  $manager = \Drupal::service('default_content.manager');

  $folder = drush_get_option('folder', '.');
  if (is_null($entity_id) && ($entities = \Drupal::entityQuery($entity_type_id)->execute())) {
    // @todo Add paging.
    foreach ($entities as $entity_id) {
      $serialized_by_type = $manager->exportContentWithReferences($entity_type_id, $entity_id);
      $manager->writeDefaultContent($serialized_by_type, $folder);
    }
  }
  else {
    $serialized_by_type = $manager->exportContentWithReferences($entity_type_id, $entity_id);
    $manager->writeDefaultContent($serialized_by_type, $folder);
  }
}

/**
 * Exports all of the content for a given module and default profile.
 *
 * @param string $module_name
 *   The module name to export.
 */
function drush_default_content_export_module($module_name) {
  if (!_drush_default_content_valid_module($module_name)) {
    return;
  }

  drush_default_content_export_modules([$module_name]);
}

/**
 * Exports all of the content for all enabled modules and default profile.
 *
 * @param array $modules
 *   The modules and/or profile to export.
 */
function drush_default_content_export_modules($modules = []) {
  if (empty($modules) || !is_array($modules)) {
    $modules = array_keys(\Drupal::config('core.extension')->get('module'));
  }

  /** @var \Drupal\default_content\DefaultContentManagerInterface $manager */
  $manager = \Drupal::service('default_content.manager');
  $total = 0;
  foreach ($modules as $module) {
    $serialized_by_type = $manager->exportModuleContent($module);
    $module_folder = \Drupal::moduleHandler()->getModule($module)->getPath() . '/content';
    $manager->writeDefaultContent($serialized_by_type, $module_folder);
    $count = 0;
    foreach ($serialized_by_type as $entity_type => $serialized_entities) {
      foreach ($serialized_entities as $uuid => $serialized_entity) {
        $count++;
      }
    }
    if ($count > 0) {
      $message = new PluralTranslatableMarkup($count, '@module: 1 entry exported.', '@module: @count entries exported.', ['@module' => $module]);
      drush_log($message, LogLevel::OK);
    }
    $total += $count;
  }
  if ($total) {
    $message = new PluralTranslatableMarkup($total, 'Total: 1 entry exported.', 'Total: @count entries exported.');
    drush_log($message, LogLevel::OK);
  }
  else {
    drush_log(dt('No content has been exported.'), LogLevel::WARNING);
  }
}

/**
 * Imports content provided in enabled modules and default profile.
 *
 * @param string $module_name
 *   The import will be limited to this module or profile.
 */
function drush_default_content_import_module($module_name) {
  if (!_drush_default_content_valid_module($module_name)) {
    return;
  }

  drush_default_content_import_modules([$module_name]);
}

/**
 * Imports of the content provided in enabled modules and default profile.
 *
 * @param array $modules
 *   The modules and/or profile to import.
 */
function drush_default_content_import_modules($modules = []) {
  if (empty($modules) || !is_array($modules)) {
    $modules = array_keys(\Drupal::config('core.extension')->get('module'));
    $profile = drupal_get_profile();
    if ($profile) {
      $modules = array_unique($modules + [$profile]);
    }
  }

  $update_existing = drush_get_option('update', FALSE);
  $default_content_manager = \Drupal::service('default_content.manager');
  $total = 0;
  foreach ($modules as $module) {
    if ($count = count($default_content_manager->importContent($module, $update_existing))) {
      $message = new PluralTranslatableMarkup($count, '@module: 1 entity imported.', '@module: @count entries imported.', ['@module' => $module]);
      drush_log($message, LogLevel::OK);
      $total += $count;
    }
  }
  if ($total) {
    $message = new PluralTranslatableMarkup($total, 'Total: 1 entry imported.', 'Total: @count entries imported.');
    drush_log($message, LogLevel::OK);
  }
  else {
    drush_log(dt('No content has been imported.'), LogLevel::WARNING);
  }
}

/**
 * Validates that a module or a profile exists and is installed.
 *
 * @param string|null $module
 *   An installed module or the default profile. If not passed, the validation
 *   passes.
 *
 * @return bool
 *   TRUE if the passed module or profile name validates.
 */
function _drush_default_content_valid_module($module) {
  return \Drupal::moduleHandler()->moduleExists($module) || $module === drupal_get_profile() ?
    TRUE :
    drush_set_error('INVALID_MODULE', dt("Module or profile '@module' doesn't exist or is uninstalled.", ['@module' => $module]));
}
