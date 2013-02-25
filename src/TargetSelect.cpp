#include "TargetSelect.hpp"
#include "MemAllocHeap.hpp"
#include "Common.hpp"

namespace t2
{

struct InputSpec
{
  int m_ConfigIndex;
  int m_VariantIndex;
  int m_SubVariantIndex;
};

static bool FindHash(int* index_out, const uint32_t* hashes, int count, uint32_t hash)
{
  for (int i = 0; i < count; ++i)
  {
    if (hashes[i] == hash)
    {
      *index_out = i;
      return true;
    }
  }

  return false;
}

static bool DestructureInputSpec(const TargetSelectInput& input, InputSpec* spec, const char* name)
{
  char buffer[256];
  strncpy(buffer, name, sizeof buffer);
  buffer[(sizeof buffer) - 1] = '\0';

  // Pick apart win32-msvc-debug-default
  // config:     win32-msvc
  // variant:    debug
  // subvariant: default
  char* dash1 = strchr(buffer, '-');
  char* dash2 = dash1 ? strchr(dash1 + 1, '-') : nullptr;
  char* dash3 = dash2 ? strchr(dash2 + 1, '-') : nullptr;

  if (!dash1)
    return false;

  const char* config     = buffer;
  const char* variant    = nullptr;
  const char* subvariant = nullptr;

  if (dash2)
  {
    *dash2 = '\0';
    variant = dash2 + 1;

    if (dash3)
    {
      *dash3 = '\0';
      subvariant = dash3 + 1;
    }
  }

  if (!FindHash(&spec->m_ConfigIndex, input.m_ConfigNameHashes, input.m_ConfigCount, Djb2Hash(config)))
    return false;

  if (variant)
  {
    if (!FindHash(&spec->m_VariantIndex, input.m_VariantNameHashes, input.m_VariantCount, Djb2Hash(variant)))
      return false;
  }
  else
  {
    spec->m_VariantIndex = -1;
  }

  if (subvariant)
  {
    if (!FindHash(&spec->m_SubVariantIndex, input.m_SubVariantNameHashes, input.m_SubVariantCount, Djb2Hash(subvariant)))
      return false;
  }
  else
  {
    spec->m_SubVariantIndex = -1;
  }

  return true;
}

void SelectTargets(const TargetSelectInput& input, MemAllocHeap* heap, Buffer<TargetSpec>* output, Buffer<const char*>* target_names)
{
  if (input.m_ConfigCount > 32 || input.m_VariantCount > 32 || input.m_SubVariantCount > 32)
    Croak("too many configs/variants/subvariants -- keep it below 32");

  int spec_count = 0;
  InputSpec* specs = (InputSpec*) alloca(sizeof(InputSpec) * (input.m_InputNameCount + 1));

  uint32_t subvariant_bits = 0;
  uint32_t variant_bits    = 0;

  for (int i = 0, count = input.m_InputNameCount; i < count; ++i)
  {
    const char* input_name = input.m_InputNames[i];
    const uint32_t input_hash = Djb2Hash(input_name);
    int item_index;

    if (FindHash(&item_index, input.m_SubVariantNameHashes, input.m_SubVariantCount, input_hash))
    {
      subvariant_bits |= 1 << item_index;
    }
    else if (FindHash(&item_index, input.m_VariantNameHashes, input.m_VariantCount, input_hash))
    {
      variant_bits |= 1 << item_index;
    }
    else if (DestructureInputSpec(input, &specs[spec_count], input_name))
    {
      spec_count++;
    }
    else
    {
      BufferAppendOne(target_names, heap, input_name);
    }
  }

  if (0 == spec_count && input.m_DefaultConfigIndex >= 0)
  {
    specs[0].m_ConfigIndex     = input.m_DefaultConfigIndex;
    specs[0].m_VariantIndex    = -1;
    specs[0].m_SubVariantIndex = -1;
    spec_count = 1;
  }

  if (0 == subvariant_bits && input.m_DefaultSubVariantIndex >= 0)
    subvariant_bits |= 1 << input.m_DefaultSubVariantIndex;

  if (0 == variant_bits && input.m_DefaultVariantIndex >= 0)
    variant_bits |= 1 << input.m_DefaultVariantIndex;

  for (int i = 0; i < spec_count; ++i)
  {
    const InputSpec& spec = specs[i];

    int config_index = spec.m_ConfigIndex;

    int vbits = spec.m_VariantIndex >= 0 ? 1 << spec.m_VariantIndex : variant_bits;

    while (vbits)
    {
      int variant_index = CountTrailingZeroes(vbits);
      vbits &= ~(1 << variant_index);

      int svbits = spec.m_SubVariantIndex >= 0 ? 1 << spec.m_SubVariantIndex : subvariant_bits;

      while (svbits)
      {
        int subvariant_index = CountTrailingZeroes(svbits);
        svbits &= ~(1 << subvariant_index);

        TargetSpec spec = { config_index, variant_index, subvariant_index };

        bool found = false;

        for (size_t o = 0, count = output->m_Size; o < count; ++o)
        {
          if (output->m_Storage[o] == spec)
          {
            found = true;
            break;
          }
        }

        if (!found)
        {
          BufferAppendOne(output, heap, spec);
        }
      }
    }
  }
}

}
